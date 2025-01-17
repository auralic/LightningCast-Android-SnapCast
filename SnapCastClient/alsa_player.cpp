/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

// prototype/interface header file
#include "alsa_player.hpp"

// local headers
#include "aixlog.hpp"
#include "snap_exception.hpp"
#include "str_compat.hpp"
#include "logging.hpp"
#include "string_utils.hpp"

// 3rd party headers

// standard headers
#include <chrono>


using namespace std::chrono_literals;
using namespace std;

namespace player
{

static constexpr std::chrono::milliseconds BUFFER_TIME = 80ms;
static constexpr int PERIODS = 4;
static constexpr int MIN_PERIODS = 3;

#define exp10(x) (exp((x)*log(10)))


static constexpr auto LOG_TAG = "Alsa";
static constexpr auto DEFAULT_MIXER = "PCM";


AlsaPlayer::AlsaPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), handle_(nullptr), ctl_(nullptr), mixer_(nullptr), elem_(nullptr), sd_(io_context), timer_(io_context)
{
    auto params = utils::string::split_pairs(settings.parameter, ',', '=');
    if (params.find("buffer_time") != params.end())
        buffer_time_ = std::chrono::milliseconds(std::max(cpt::stoi(params["buffer_time"]), 10));
    if (params.find("fragments") != params.end())
        periods_ = std::max(cpt::stoi(params["fragments"]), 2);

    LOG(INFO, LOG_TAG) << "Using " << (buffer_time_.has_value() ? "configured" : "default")
                       << " buffer_time: " << buffer_time_.value_or(BUFFER_TIME).count() / 1000 << " ms, " << (periods_.has_value() ? "configured" : "default")
                       << " fragments: " << periods_.value_or(PERIODS) << "\n";
}


void AlsaPlayer::initAlsa()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const SampleFormat& format = stream_->getFormat();
    uint32_t rate = format.rate();
    int channels = format.channels();
    int err;

    // Open the PCM device in playback mode
    if ((err = snd_pcm_open(&handle_, settings_.pcm_device.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        throw SnapException("Can't open " + settings_.pcm_device.name + ", error: " + snd_strerror(err), err);

    // struct snd_pcm_playback_info_t pinfo;
    // if ((pcm = snd_pcm_playback_info( pcm_handle, &pinfo)) < 0)
    //     fprintf(stderr, "Error: playback info error: %s\n", snd_strerror(err));

    // Allocate parameters object and fill it with default values
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    if ((err = snd_pcm_hw_params_any(handle_, params)) < 0)
        throw SnapException("Can't fill params: " + string(snd_strerror(err)));

    snd_output_t* output;
    if (snd_output_buffer_open(&output) == 0)
    {
        if (snd_pcm_hw_params_dump(params, output) == 0)
        {
            char* str;
            size_t len = snd_output_buffer_string(output, &str);
            LOG(DEBUG, LOG_TAG) << std::string(str, len) << "\n";
        }
        snd_output_close(output);
    }

    // Set parameters
    if ((err = snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        throw SnapException("Can't set interleaved mode: " + string(snd_strerror(err)));

    snd_pcm_format_t snd_pcm_format;
    if (format.bits() == 8)
        snd_pcm_format = SND_PCM_FORMAT_S8;
    else if (format.bits() == 16)
        snd_pcm_format = SND_PCM_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        snd_pcm_format = SND_PCM_FORMAT_S24_LE;
    else if (format.bits() == 32)
        snd_pcm_format = SND_PCM_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));

    err = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
    if (err == -EINVAL)
    {
        if (snd_pcm_format == SND_PCM_FORMAT_S24_LE)
        {
            snd_pcm_format = SND_PCM_FORMAT_S32_LE;
            volCorrection_ = 256;
        }
        if (snd_pcm_format == SND_PCM_FORMAT_S8)
        {
            snd_pcm_format = SND_PCM_FORMAT_U8;
        }
    }

    err = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
    if (err < 0)
    {
        stringstream ss;
        ss << "Can't set format: " << string(snd_strerror(err)) << ", supported: ";
        for (int format = 0; format <= static_cast<int>(SND_PCM_FORMAT_LAST); format++)
        {
            auto snd_pcm_format = static_cast<snd_pcm_format_t>(format);
            if (snd_pcm_hw_params_test_format(handle_, params, snd_pcm_format) == 0)
                ss << snd_pcm_format_name(snd_pcm_format) << " ";
        }
        throw SnapException(ss.str());
    }

    if ((err = snd_pcm_hw_params_set_channels(handle_, params, channels)) < 0)
        throw SnapException("Can't set channel count: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, nullptr)) < 0)
        throw SnapException("Can't set rate: " + string(snd_strerror(err)));
    if (rate != format.rate())
        LOG(WARNING, LOG_TAG) << "Could not set sample rate to " << format.rate() << " Hz, using: " << rate << " Hz\n";

    uint32_t period_time = buffer_time_.value_or(BUFFER_TIME).count() / periods_.value_or(PERIODS);
    LOG(INFO, LOG_TAG) << "period_time========== " << period_time << "\n"; 
    uint32_t max_period_time = period_time;
    if ((err = snd_pcm_hw_params_get_period_time_max(params, &max_period_time, nullptr)) < 0)
    {
        LOG(ERROR, LOG_TAG) << "Can't get max period time: " << snd_strerror(err) << "\n";
    }
    else
    {
        if (period_time > max_period_time)
        {
            LOG(INFO, LOG_TAG) << "Period time too large, changing from " << period_time << " to " << max_period_time << "\n";
            period_time = max_period_time;
        }
    }
    uint32_t min_period_time = period_time;
    if ((err = snd_pcm_hw_params_get_period_time_min(params, &min_period_time, nullptr)) < 0)
    {
        LOG(ERROR, LOG_TAG) << "Can't get min period time: " << snd_strerror(err) << "\n";
    }
    else
    {
        if (period_time < min_period_time)
        {
            LOG(INFO, LOG_TAG) << "Period time too small, changing from " << period_time << " to " << min_period_time << "\n";
            period_time = min_period_time;
        }
    }

    if ((err = snd_pcm_hw_params_set_period_time_near(handle_, params, &period_time, nullptr)) < 0)
        throw SnapException("Can't set period time: " + string(snd_strerror(err)));

    uint32_t buffer_time = buffer_time_.value_or(BUFFER_TIME).count();
    LOG(INFO, LOG_TAG) << "buffer_time========== " << buffer_time << "\n"; 
    uint32_t periods = periods_.value_or(MIN_PERIODS);
    LOG(INFO, LOG_TAG) << "periods========== " << periods << "\n"; 
    if (buffer_time < period_time * periods)
    {
        LOG(INFO, LOG_TAG) << "Buffer time smaller than " << periods << " * periods: " << buffer_time << " us < " << period_time * periods
                           << " us, raising buffer time\n";
        buffer_time = period_time * periods;
    }

    if ((err = snd_pcm_hw_params_set_buffer_time_near(handle_, params, &buffer_time, nullptr)) < 0)
        throw SnapException("Can't set buffer time to " + cpt::to_string(buffer_time) + " us : " + string(snd_strerror(err)));

    // unsigned int periods = periods_;
    // if ((err = snd_pcm_hw_params_set_periods_near(handle_, params, &periods, 0)) < 0)
    //     throw SnapException("Can't set periods: " + string(snd_strerror(err)));

    // Write parameters
    if ((err = snd_pcm_hw_params(handle_, params)) < 0)
        throw SnapException("Can't set hardware parameters: " + string(snd_strerror(err)));

    // Resume information
    // uint32_t periods;
    if (snd_pcm_hw_params_get_periods(params, &periods, nullptr) < 0)
        periods = round(static_cast<double>(buffer_time) / static_cast<double>(period_time));
    snd_pcm_hw_params_get_period_size(params, &frames_, nullptr);
    LOG(INFO, LOG_TAG) << "PCM name: " << snd_pcm_name(handle_) << ", sample rate: " << rate << " Hz, channels: " << channels
                       << ", buffer time: " << buffer_time << " us, periods: " << periods << ", period time: " << period_time
                       << " us, period frames: " << frames_ << "\n";

    // Allocate buffer to hold single period
    snd_pcm_sw_params_t* swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(handle_, swparams);

    snd_pcm_sw_params_set_avail_min(handle_, swparams, frames_);
    snd_pcm_sw_params_set_start_threshold(handle_, swparams, frames_);
    //	snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, frames_);
    snd_pcm_sw_params(handle_, swparams);

    if (snd_pcm_state(handle_) == SND_PCM_STATE_PREPARED)
    {
        if ((err = snd_pcm_start(handle_)) < 0)
            LOG(DEBUG, LOG_TAG) << "Failed to start PCM: " << snd_strerror(err) << "\n";
    }
}


void AlsaPlayer::uninitAlsa(bool uninit_mixer)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (handle_ != nullptr)
    {
        snd_pcm_drop(handle_);
        snd_pcm_close(handle_);
        handle_ = nullptr;
    }
}

void AlsaPlayer::start()
{
    try
    {
        initAlsa();
    }
    catch (const SnapException& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << ", code: " << e.code() << "\n";
        // Accept "Device or ressource busy", the worker loop will retry
        if (e.code() != -EBUSY)
            throw;
    }

    Player::start();
}


AlsaPlayer::~AlsaPlayer()
{
    stop();
}


void AlsaPlayer::stop()
{
    Player::stop();
    uninitAlsa(true);
}


bool AlsaPlayer::needsThread() const
{
    return true;
}


bool AlsaPlayer::getAvailDelay(snd_pcm_sframes_t& avail, snd_pcm_sframes_t& delay)
{
    int result = snd_pcm_avail_delay(handle_, &avail, &delay);
    if (result < 0)
    {
        LOG(WARNING, LOG_TAG) << "snd_pcm_avail_delay failed: " << snd_strerror(result) << " (" << result << "), avail: " << avail << ", delay: " << delay
                              << ", using snd_pcm_avail amd snd_pcm_delay.\n";
        this_thread::sleep_for(1ms);
        avail = snd_pcm_avail(handle_);
        result = snd_pcm_delay(handle_, &delay);
        if ((result < 0) || (delay < 0))
        {
            LOG(WARNING, LOG_TAG) << "snd_pcm_delay failed: " << snd_strerror(result) << " (" << result << "), avail: " << avail << ", delay: " << delay
                                  << "\n";
            return false;
        }
        // LOG(DEBUG, LOG_TAG) << "snd_pcm_delay: " << delay << ", snd_pcm_avail: " << avail << "\n";
    }

    if (avail < 0)
    {
        LOG(DEBUG, LOG_TAG) << "snd_pcm_avail failed: " << snd_strerror(avail) << " (" << avail << "), using " << frames_ << "\n";
        avail = frames_;
    }

    return true;
}


void AlsaPlayer::worker()
{
    snd_pcm_sframes_t pcm;
    snd_pcm_sframes_t framesDelay;
    snd_pcm_sframes_t framesAvail;
    long lastChunkTick = chronos::getTickCount();
    const SampleFormat& format = stream_->getFormat();
    while (active_)
    {
        if (handle_ == nullptr)
        {
            try
            {
                initAlsa();
            }
            catch (const std::exception& e)
            {
                LOG(ERROR, LOG_TAG) << "Exception in initAlsa: " << e.what() << endl;
                chronos::sleep(100);
            }
            if (handle_ == nullptr)
                continue;
        }

        int wait_result = snd_pcm_wait(handle_, 100);
        if (wait_result == -EPIPE)
        {
            LOG(ERROR, LOG_TAG) << "XRUN while waiting for PCM: " << snd_strerror(wait_result) << "\n";
            snd_pcm_prepare(handle_);
        }
        else if (wait_result < 0)
        {
            LOG(ERROR, LOG_TAG) << "ERROR. Can't wait for PCM to become ready: " << snd_strerror(wait_result) << "\n";
            uninitAlsa(true);
            continue;
        }
        else if (wait_result == 0)
        {
            continue;
        }

        if (!getAvailDelay(framesAvail, framesDelay))
        {
            this_thread::sleep_for(10ms);
            snd_pcm_prepare(handle_);
            continue;
        }

        // if (framesAvail < static_cast<snd_pcm_sframes_t>(frames_))
        // {
        //     this_thread::sleep_for(5ms);
        //     continue;
        // }
        if (framesAvail == 0)
        {
            auto frame_time = std::chrono::microseconds(static_cast<int>(frames_ / format.usRate()));
            std::chrono::microseconds wait = std::min(frame_time / 2, std::chrono::microseconds(10ms));
            LOG(DEBUG, LOG_TAG) << "No frames available, waiting for " << wait.count() << " us\n";
            this_thread::sleep_for(wait);
            continue;
        }

        // LOG(TRACE, LOG_TAG) << "res: " << result << ", framesAvail: " << framesAvail << ", delay: " << framesDelay << ", frames: " << frames_ << "\n";
        chronos::usec delay(static_cast<chronos::usec::rep>(1000 * static_cast<double>(framesDelay) / format.msRate()));
        // LOG(TRACE, LOG_TAG) << "delay: " << framesDelay << ", delay[ms]: " << delay.count() / 1000 << ", avail: " << framesAvail << "\n";

        if (buffer_.size() < static_cast<size_t>(framesAvail * format.frameSize()))
        {
            LOG(DEBUG, LOG_TAG) << "Resizing buffer from " << buffer_.size() << " to " << framesAvail * format.frameSize() << "\n";
            buffer_.resize(framesAvail * format.frameSize());
        }
        if (stream_->getPlayerChunk(buffer_.data(), delay, framesAvail))
        {
            lastChunkTick = chronos::getTickCount();
            if ((pcm = snd_pcm_writei(handle_, buffer_.data(), framesAvail)) == -EPIPE)
            {
                LOG(ERROR, LOG_TAG) << "XRUN while writing to PCM: " << snd_strerror(pcm) << "\n";
                snd_pcm_prepare(handle_);
            }
            else if (pcm < 0)
            {
                LOG(ERROR, LOG_TAG) << "ERROR. Can't write to PCM device: " << snd_strerror(pcm) << "\n";
                uninitAlsa(true);
            }
        }
        else
        {
            LOG(INFO, LOG_TAG) << "Failed to get chunk\n";
            while (active_ && !stream_->waitForChunk(100ms))
            {
                // Log "Waiting for chunk" only every second second
                static utils::logging::TimeConditional cond(2s);
                LOG(DEBUG, LOG_TAG) << cond << "Waiting for chunk\n";
                if ((handle_ != nullptr) && (chronos::getTickCount() - lastChunkTick > 5000))
                {
                    LOG(NOTICE, LOG_TAG) << "No chunk received for 5000ms. Closing ALSA.\n";
                    uninitAlsa(false);
                    stream_->clearChunks();
                }
            }
        }
    }
}



vector<PcmDevice> AlsaPlayer::pcm_list()
{
    void **hints, **n;
    char *name, *descr, *io;
    vector<PcmDevice> result;
    PcmDevice pcmDevice;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return result;
    n = hints;
    size_t idx(0);
    while (*n != nullptr)
    {
        name = snd_device_name_get_hint(*n, "NAME");
        descr = snd_device_name_get_hint(*n, "DESC");
        io = snd_device_name_get_hint(*n, "IOID");
        if (io != nullptr && strcmp(io, "Output") != 0)
            goto __end;
        pcmDevice.name = name;
        if (descr == nullptr)
        {
            pcmDevice.description = "";
        }
        else
        {
            pcmDevice.description = descr;
        }
        pcmDevice.idx = idx++;
        result.push_back(pcmDevice);

    __end:
        if (name != nullptr)
            free(name);
        if (descr != nullptr)
            free(descr);
        if (io != nullptr)
            free(io);
        n++;
    }
    snd_device_name_free_hint(hints);
    return result;
}

} // namespace player