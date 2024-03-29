#include "stdafx.h"

extern cfg_uint cfg_fs, cfg_channel;
extern cfg_int cfg_bits;

// Note that input class does *not* implement virtual methods or derive from interface classes.
// Our methods get called over input framework templates. See input_singletrack_impl for descriptions of what each method does.
// input_stubs just provides stub implementations of mundane methods that are irrelevant for most implementations.
class input_raw : public input_stubs {
	unsigned raw_sample_rate;
	unsigned raw_channels;
	unsigned raw_bits_per_sample;
	unsigned raw_total_sample_width;

public:
	input_raw() {
		raw_sample_rate = cfg_fs;
		raw_channels = cfg_channel;
		raw_bits_per_sample = std::abs(cfg_bits);
		raw_total_sample_width = raw_bits_per_sample / 8 * raw_channels;
	}

	void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {
		if (p_reason == input_open_info_write) throw exception_tagging_unsupported();//our input does not support retagging.
		m_file = p_filehint;//p_filehint may be null, hence next line
		input_open_file_helper(m_file,p_path,p_reason,p_abort);//if m_file is null, opens file with appropriate privileges for our operation (read/write for writing tags, read-only otherwise).
	}

	void get_info(file_info & p_info,abort_callback & p_abort) {
		t_filesize size = m_file->get_size(p_abort);
		//note that the file size is not always known, for an example, live streams and alike have no defined size and filesize_invalid is returned
		if (size != filesize_invalid) {
			//file size is known, let's set length
			p_info.set_length(audio_math::samples_to_time( size / raw_total_sample_width, raw_sample_rate));
		}
		//note that the values below should be based on contents of the file itself, NOT on user-configurable variables for an example. To report info that changes independently from file contents, use get_dynamic_info/get_dynamic_info_track instead.
		p_info.info_set_int("samplerate",raw_sample_rate);
		p_info.info_set_int("channels",raw_channels);
		p_info.info_set_int("bitspersample",raw_bits_per_sample);

		// Indicate whether this is a fixedpoint or floatingpoint stream, when using bps >= 32
		// As 32bit fixedpoint can't be decoded losslessly by fb2k, does not fit in float32 audio_sample.
		if (cfg_bits >= 32) p_info.info_set("bitspersample_extra", "fixed-point");
		if (cfg_bits < 0) p_info.info_set("bitspersample_extra", "floating-point");

		p_info.info_set("encoding","lossless");
		p_info.info_set_bitrate((raw_bits_per_sample * raw_channels * raw_sample_rate + 500 /* rounding for bps to kbps*/ ) / 1000 /* bps to kbps */);
		
	}
	t_filestats2 get_stats2(unsigned f, abort_callback& a) {return m_file->get_stats2_(f, a);}
	t_filestats get_file_stats(abort_callback & p_abort) {return m_file->get_stats(p_abort);}

	void decode_initialize(unsigned p_flags,abort_callback & p_abort) {
		m_file->reopen(p_abort);//equivalent to seek to zero, except it also works on nonseekable streams
	}
	bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
		enum {
			deltaread = 1024,
		};

		const size_t deltaReadBytes = deltaread * raw_total_sample_width;
		// Prepare buffer
		m_buffer.set_size(deltaReadBytes);
		// Read bytes
		size_t got = m_file->read(m_buffer.get_ptr(), deltaReadBytes,p_abort) / raw_total_sample_width;

		// EOF?
		if (got == 0) return false;

		if (0 < cfg_bits) {
			// This converts the data that we've read to the audio_chunk's internal format, audio_sample (float 32-bit).
			// audio_sample is the audio data format that all fb2k code works with.
			p_chunk.set_data_fixedpoint(m_buffer.get_ptr(), got * raw_total_sample_width, raw_sample_rate, raw_channels, raw_bits_per_sample, audio_chunk::g_guess_channel_config(raw_channels));
		}
		else {
			p_chunk.set_data((const audio_sample*)m_buffer.get_ptr(), got, raw_channels, raw_sample_rate);
		}

		//processed successfully, no EOF
		return true;
	}
	void decode_seek(double p_seconds,abort_callback & p_abort) {
		m_file->ensure_seekable();//throw exceptions if someone called decode_seek() despite of our input having reported itself as nonseekable.
		// IMPORTANT: convert time to sample offset with proper rounding! audio_math::time_to_samples does this properly for you.
		t_filesize target = audio_math::time_to_samples(p_seconds,raw_sample_rate) * raw_total_sample_width;
		
		// get_size_ex fails (throws exceptions) if size is not known (where get_size would return filesize_invalid). Should never fail on seekable streams (if it does it's not our problem anymore).
		t_filesize max = m_file->get_size_ex(p_abort);
		if (target > max) target = max;//clip seek-past-eof attempts to legal range (next decode_run() call will just signal EOF).

		m_file->seek(target,p_abort);
	}
	bool decode_can_seek() {return m_file->can_seek();}
	bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {return false;} // deals with dynamic information such as VBR bitrates
	bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {
		return true;
	} // deals with dynamic information such as track changes in live streams
	void decode_on_idle(abort_callback & p_abort) {m_file->on_idle(p_abort);}

	// Note that open() already rejects requests to open for tag writing, so these two should never get called.
	void retag(const file_info & p_info,abort_callback & p_abort) {throw exception_tagging_unsupported();}
	void remove_tags(abort_callback&) { throw exception_tagging_unsupported(); }

	static bool g_is_our_content_type(const char * p_content_type) {return false;} // match against supported mime types here
	static bool g_is_our_path(const char * p_path, const char * p_extension) {
		return 0 == stricmp_utf8(p_extension, "pcm") || 0 == stricmp_utf8(p_extension, "raw");
	}
	static const char * g_get_name() { return "foo_input_raw raw input"; }
	static const GUID g_get_guid() {
		return { 0x5060dd5f, 0x16a4, 0x4a9f, { 0x93, 0x6f, 0x1a, 0xd2, 0xd6, 0x22, 0x5a, 0xf8 } };
	}

private:
	service_ptr_t<file> m_file;
	pfc::array_t<t_uint8> m_buffer;
};

static input_singletrack_factory_t<input_raw> g_input_raw_factory;

// Declare .RAW as a supported file type to make it show in "open file" dialog etc.
DECLARE_FILE_TYPE("Raw files","*.RAW;*.PCM");
