#pragma once
//if long comments quoting standard bother you,
//use /\*(.|\n)*?\*/ regex command to remove them

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

#include <fstream>

#ifndef M_TAU
#define M_TAU 6.28318530717958647692
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "RIFF.hpp"
#include "DynamicPool.hpp"

#ifdef SF2_DEBUG
#define SF2_DEBUG_OUTPUT(msg) {printf(msg);}
#else
#define SF2_DEBUG_OUTPUT(msg) {}
#endif

namespace SF2
{
	typedef uint8_t byte;
	typedef uint16_t word;
	typedef uint32_t doubleword;
	typedef int8_t CHAR;
	typedef uint8_t BYTE;
	typedef int16_t SHORT;
	typedef uint16_t WORD;
	typedef uint32_t DWORD;

	double fastPow(double a, double b) {
		union {
			double d;
			int x[2];
		} u = { a };
		u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
		u.x[0] = 0;
		return u.d;
	}

	inline float cents_to_hertz(float cents)
	{
		return std::pow(2.0f, cents / 1200.0f);
	}
	inline float hertz_to_cents(float hz)
	{
		return 1200.0f * std::log2(hz);
	}
	inline float decibels_to_gain(float db)
	{
		//return (db > -100.0f)?std::pow(10.0f, db * 0.05f):0.0f;		
		return (db > -100.0f)?fastPow(10.0f, db * 0.05f):0.0f;
	}
	inline float gain_to_decibels(float gain)
	{
		return (gain <= 0.00001f)?-100.0f:(20.0f * std::log10(gain));
	}
	inline float calc_interval_cents(float hz1, float hz2)
	{
		return hertz_to_cents(hz2/hz1);
	}
	inline float apply_interval_cents(float hz, float cents)
	{
		return hz*cents_to_hertz(cents);
	}
	float timecents_to_seconds(float timecents)
	{		
		//return (timecents == -32768)?0.000001f:std::pow(2.0f, timecents / 1200.0f);
		return (timecents <= -12000)?0.001f:std::pow(2.0f, timecents / 1200.0f);
	}
	float seconds_to_timecents(float seconds)
	{
		return 1200.0f*std::log2(seconds);
	}
	inline void constant_power_pan(float& factor_L, float& factor_R, float pan)
	{
		//constant is sqrt(2.0)/2.0
		constexpr const float sqrt2_2 = 0.70710678f;
		pan = pan*M_TAU*0.125f;
		factor_L = sqrt2_2*(cos(pan)-sin(pan));
		factor_R = sqrt2_2*(cos(pan)+sin(pan));
	}
	inline float clamp_panning(float pan)
	{
		pan = std::max(std::min(pan, 2.0f), -2.0f);
		pan = (pan > 1.0f)?(2.0f-pan):((pan < -1.0f)?-(-2.0f+pan):pan);
		return pan;
	}
	inline float lerp(float a, float b, float f)
	{
		return (a * (1.0f - f)) + (b * f);
	}
	//algebraically simplified algorithm, less precise
	inline float fast_lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}
	//input must be in range [0, 127]
	//might as well precompute a table for it later
	//TODO: write a program to precompute 127 values for convex curve
	inline float convex_curve(float input)
	{
		if(input == 0.0f) return 0.0f;//mathematically undefined
		return 1.0f+20.0f*std::log10((input*input)/(127.0f*127.0f))/96.0f;
	}
	inline float concave_curve(float input)
	{
		return 1.0f-convex_curve(127.0f-input);
	}

	//SoundFont2-structured RIFF
	//I was planning to use this for editing, but...
	struct RIFF_SoundFont2
	{
		//Supplemental Information
		struct INFO_chunk
		{
			RIFF::RIFF::chunk* ifil;
			//Refers to the target Sound Engine
			RIFF::RIFF::chunk* isng;
			//Refers to the Sound Font Bank Name
			RIFF::RIFF::chunk* INAM;
			//Refers to the Sound ROM Name
			RIFF::RIFF::chunk* irom;
			//Refers to the Sound ROM Version
			RIFF::RIFF::chunk* iver;
			//Refers to the Date of Creation of the Bank
			RIFF::RIFF::chunk* ICRD;
			//Sound Designers and Engineers for the Bank
			RIFF::RIFF::chunk* IENG;
			//Product for which the Bank was intended
			RIFF::RIFF::chunk* IPRD;
			//Contains any Copyright message
			RIFF::RIFF::chunk* ICOP;
			//Contains any Comments on the Bank
			RIFF::RIFF::chunk* ICMT;
			//The SoundFont tools used to create and alter the bank
			RIFF::RIFF::chunk* ISFT;
		};
		INFO_chunk INFO;

		//The Sample Binary Data
		struct sdta_chunk
		{
			//The Digital Audio Samples for the upper 16 bits
			RIFF::RIFF::chunk* smpl;
			//The Digital Audio Samples for the lower 8 bits
			RIFF::RIFF::chunk* sm24;
		};
		sdta_chunk sdta;

		//The Preset, Instrument, and Sample Header data
		struct pdta_chunk
		{
			//The Preset Headers
			RIFF::RIFF::chunk* phdr;
			//The Preset Index list
			RIFF::RIFF::chunk* pbag;
			//The Preset Modulator list
			RIFF::RIFF::chunk* pmod;
			//The Preset Generator list
			RIFF::RIFF::chunk* pgen;
			//The Instrument Names and Indices
			RIFF::RIFF::chunk* inst;
			//The Instrument Index list
			RIFF::RIFF::chunk* ibag;
			//The Instrument Modulator list
			RIFF::RIFF::chunk* imod;
			//The Instrument Generator list
			RIFF::RIFF::chunk* igen;
			//The Sample Headers
			RIFF::RIFF::chunk* shdr;
		};
		pdta_chunk pdta;

		bool structurally_unsound = false;

		RIFF_SoundFont2(RIFF::RIFF* riff)
		{
#define cond_check(cond)\
			{\
				if(cond)\
				{\
					structurally_unsound = true;\
					return;\
				}\
			}
#define get_subchunk(chunk, riff, id, start_index)\
			{\
				chunk.id = riff->get_chunk_by_id(\
					RIFF::string_to_FOURCC(#id),\
					start_index\
				);\
			}
			auto& chunks = riff->chunks;
			cond_check(chunks.empty());
			//Check RIFF chunk, must be first
			cond_check(!RIFF::FOURCC_equals(chunks[0]->id, "RIFF") || !RIFF::FOURCC_equals(chunks[0]->type, "sfbk"));
			//Get INFO-list chunk
			auto INFO_list_index = riff->get_chunk_index_by_id_type(
				RIFF::string_to_FOURCC("LIST"),
				RIFF::string_to_FOURCC("INFO")
			);
			//Check INFO-list chunk, must exist
			cond_check(INFO_list_index == -1);
			//Get INFO-list subchunks
			get_subchunk(INFO, riff, ifil, INFO_list_index);
			get_subchunk(INFO, riff, isng, INFO_list_index);
			get_subchunk(INFO, riff, INAM, INFO_list_index);
			get_subchunk(INFO, riff, irom, INFO_list_index);
			get_subchunk(INFO, riff, iver, INFO_list_index);
			get_subchunk(INFO, riff, ICRD, INFO_list_index);
			get_subchunk(INFO, riff, IENG, INFO_list_index);
			get_subchunk(INFO, riff, IPRD, INFO_list_index);
			get_subchunk(INFO, riff, ICOP, INFO_list_index);
			get_subchunk(INFO, riff, ICMT, INFO_list_index);
			get_subchunk(INFO, riff, ISFT, INFO_list_index);
			//Check mandatory subchunks
			cond_check(!INFO.ifil);

			//Get sdta chunk
			auto sdta_index = riff->get_chunk_index_by_id_type(
				RIFF::string_to_FOURCC("LIST"),
				RIFF::string_to_FOURCC("sdta")
			);
			//Check sdta chunk, must exist
			cond_check(sdta_index == -1);
			//Get sdta subchunks
			get_subchunk(sdta, riff, smpl, sdta_index);
			get_subchunk(sdta, riff, sm24, sdta_index);

			//Get pdta chunk
			auto pdta_index = riff->get_chunk_index_by_id_type(
				RIFF::string_to_FOURCC("LIST"),
				RIFF::string_to_FOURCC("sdta")
			);
			//Check pdta chunk, must exist
			cond_check(pdta_index == -1);
			//Get pdta subchunks
			get_subchunk(pdta, riff, phdr, pdta_index);
			get_subchunk(pdta, riff, pbag, pdta_index);
			get_subchunk(pdta, riff, pmod, pdta_index);
			get_subchunk(pdta, riff, pgen, pdta_index);
			get_subchunk(pdta, riff, inst, pdta_index);
			get_subchunk(pdta, riff, ibag, pdta_index);
			get_subchunk(pdta, riff, imod, pdta_index);
			get_subchunk(pdta, riff, igen, pdta_index);
			get_subchunk(pdta, riff, shdr, pdta_index);

			//Check chunk sizes
			cond_check(pdta.phdr->size % 38);
			cond_check(pdta.pbag->size % 4);
			cond_check(pdta.pmod->size % 10);
			cond_check(pdta.pgen->size % 4);
			cond_check(pdta.inst->size % 22);
			cond_check(pdta.ibag->size % 4);
			cond_check(pdta.imod->size % 10);
			cond_check(pdta.igen->size % 4);
			cond_check(pdta.shdr->size % 46);

#undef check
#undef get_subchunk
		}
	};

	struct SoundFont2
	{
		struct rangesType
		{
			BYTE byLo;
			BYTE byHi;

			//expand
			inline rangesType& operator+=(const rangesType& rhs)
			{
				byLo = std::min(byLo, rhs.byLo);
				byHi = std::max(byHi, rhs.byHi);
				return *this;
			}
			//expand
			friend inline rangesType operator+(rangesType lhs, const rangesType& rhs)
			{
				lhs += rhs;
				return lhs;
			}
			//intersect
			inline rangesType& operator*=(const rangesType& rhs)
			{
				byLo = std::max(byLo, rhs.byLo);
				byHi = std::min(byHi, rhs.byHi);
				return *this;
			}
			//intersect
			friend inline rangesType operator*(rangesType lhs, const rangesType& rhs)
			{
				lhs *= rhs;
				return lhs;
			}
		};
		typedef union
		{
			rangesType ranges;
			SHORT shAmount;
			WORD wAmount;
		} genAmountType; 
		typedef enum
		{
			monoSample = 1,
			rightSample = 2,
			leftSample = 4,
			linkedSample = 8,
			RomMonoSample = 0x8001,
			RomRightSample = 0x8002,
			RomLeftSample = 0x8004,
			RomLinkedSample = 0x8008
		} SFSampleLink;
		inline bool IsSampleROM(SFSampleLink type) {return type & 0xFFF0;};
		inline bool CheckSampleLinkType(SFSampleLink type)
		{
			return (type >= 1 && type <= 8) || (type >= 0x8001 && type <= 0x8008);
		};

		struct SFModulator
		{
			WORD enumeration;
		};
		struct SFGenerator
		{
			enum class GenType : WORD
			{
				/*The offset, in sample data points, beyond the Start sample header parameter to the
				first sample data point to be played for this instrument. For example, if Start were 7
				and startAddrOffset were 2, the first sample data point played would be sample data
				point 9. */
				startAddrsOffset = 0,

				/*The offset, in sample sample data points, beyond the End sample header parameter to
				the last sample data point to be played for this instrument. For example, if End were
				17 and endAddrOffset were -2, the last sample data point played would be sample
				data point 15.*/
				endAddrsOffset = 1,

				/*The offset, in sample data points, beyond the Startloop sample header parameter to
				the first sample data point to be repeated in the loop for this instrument. For
				example, if Startloop were 10 and startloopAddrsOffset were -1, the first repeated
				loop sample data point would be sample data point 9.*/
				startloopAddrsOffset = 2,

				/*The offset, in sample data points, beyond the Endloop sample header parameter to
				the sample data point considered equivalent to the Startloop sample data point for the
				loop for this instrument. For example, if Endloop were 15 and endloopAddrsOffset
				were 2, sample data point 17 would be considered equivalent to the Startloop sample
				data point, and hence sample data point 16 would effectively precede Startloop
				during looping. */
				endloopAddrsOffset = 3,

				/*The offset, in 32768 sample data point increments beyond the Start sample header
				parameter and the first sample data point to be played in this instrument. This
				parameter is added to the startAddrsOffset parameter. For example, if Start were 5, 
				startAddrsOffset were 3 and startAddrsCoarseOffset were 2, the first sample data
				point played would be sample data point 65544. */
				startAddrsCoarseOffset = 4,

				/*This is the degree, in cents, to which a full scale excursion of the Modulation LFO
				will influence pitch. A positive value indicates a positive LFO excursion increases
				pitch; a negative value indicates a positive excursion decreases pitch. Pitch is always
				modified logarithmically, that is the deviation is in cents, semitones, and octaves
				rather than in Hz. For example, a value of 100 indicates that the pitch will first rise 1
				semitone, then fall one semitone. */
				modLfoToPitch = 5,

				/*This is the degree, in cents, to which a full scale excursion of the Vibrato LFO will
				influence pitch. A positive value indicates a positive LFO excursion increases pitch;
				a negative value indicates a positive excursion decreases pitch. Pitch is always
				modified logarithmically, that is the deviation is in cents, semitones, and octaves
				rather than in Hz. For example, a value of 100 indicates that the pitch will first rise 1
				semitone, then fall one semitone. */
				vibLfoToPitch = 6,

				/*This is the degree, in cents, to which a full scale excursion of the Modulation
				Envelope will influence pitch. A positive value indicates an increase in pitch; a
				negative value indicates a decrease in pitch. Pitch is always modified
				logarithmically, that is the deviation is in cents, semitones, and octaves rather than in
				Hz. For example, a value of 100 indicates that the pitch will rise 1 semitone at the
				envelope peak. */
				modEnvToPitch = 7,

				/*This is the cutoff and resonant frequency of the lowpass filter in absolute cent units.
				The lowpass filter is defined as a second order resonant pole pair whose pole
				frequency in Hz is defined by the Initial Filter Cutoff parameter. When the cutoff
				frequency exceeds 20kHz and the Q (resonance) of the filter is zero, the filter does
				not affect the signal. */
				initialFilterFc = 8,

				/*This is the height above DC gain in centibels which the filter resonance exhibits at
				the cutoff frequency. A value of zero or less indicates the filter is not resonant; the
				gain at the cutoff frequency (pole angle) may be less than zero when zero is
				specified. The filter gain at DC is also affected by this parameter such that the gain
				at DC is reduced by half the specified gain. For example, for a value of 100, the
				filter gain at DC would be 5 dB below unity gain, and the height of the resonant peak
				would be 10 dB above the DC gain, or 5 dB above unity gain. Note also that if
				initialFilterQ is set to zero or less and the cutoff frequency exceeds 20 kHz, then the
				filter response is flat and unity gain. */
				initialFilterQ = 9,

				/*This is the degree, in cents, to which a full scale excursion of the Modulation LFO
				will influence filter cutoff frequency. A positive number indicates a positive LFO
				excursion increases cutoff frequency; a negative number indicates a positive
				excursion decreases cutoff frequency. Filter cutoff frequency is always modified
				logarithmically, that is the deviation is in cents, semitones, and octaves rather than in
				Hz. For example, a value of 1200 indicates that the cutoff frequency will first rise 1
				octave, then fall one octave. */
				modLfoToFilterFc = 10,

				/*This is the degree, in cents, to which a full scale excursion of the Modulation
				Envelope will influence filter cutoff frequency. A positive number indicates an
				increase in cutoff frequency; a negative number indicates a decrease in filter cutoff
				frequency. Filter cutoff frequency is always modified logarithmically, that is the
				deviation is in cents, semitones, and octaves rather than in Hz. For example, a value
				of 1000 indicates that the cutoff frequency will rise one octave at the envelope attack
				peak. */
				modEnvToFilterFc = 11,

				/*The offset, in 32768 sample data point increments beyond the End sample header
				parameter and the last sample data point to be played in this instrument. This
				parameter is added to the endAddrsOffset parameter. For example, if End were
				65536, startAddrsOffset were -3 and startAddrsCoarseOffset were -1, the last sample
				data point played would be sample data point 32765. */
				endAddrsCoarseOffset = 12,

				/*This is the degree, in centibels, to which a full scale excursion of the Modulation
				LFO will influence volume. A positive number indicates a positive LFO excursion
				increases volume; a negative number indicates a positive excursion decreases
				volume. Volume is always modified logarithmically, that is the deviation is in
				decibels rather than in linear amplitude. For example, a value of 100 indicates that
				the volume will first rise ten dB, then fall ten dB. */
				modLfoToVolume = 13,

				/*Unused, reserved. Should be ignored if encountered. */
				unused1 = 14,

				/*This is the degree, in 0.1% units, to which the audio output of the note is sent to the
				chorus effects processor. A value of 0% or less indicates no signal is sent from this
				note; a value of 100% or more indicates the note is sent at full level. Note that this
				parameter has no effect on the amount of this signal sent to the dry or unprocessed
				portion of the output. For example, a value of 250 indicates that the signal is sent at
				25% of full level (attenuation of 12 dB from full level) to the chorus effects
				processor. */
				chorusEffectsSend = 15,

				/*This is the degree, in 0.1% units, to which the audio output of the note is sent to the
				reverb effects processor. A value of 0% or less indicates no signal is sent from this
				note; a value of 100% or more indicates the note is sent at full level. Note that this
				parameter has no effect on the amount of this signal sent to the dry or unprocessed
				portion of the output. For example, a value of 250 indicates that the signal is sent at
				25% of full level (attenuation of 12 dB from full level) to the reverb effects
				processor. */
				reverbEffectsSend = 16,

				/*This is the degree, in 0.1% units, to which the dry audio output of the note is
				positioned to the left or right output. A value of -50% or less indicates the signal is
				sent entirely to the left output and not sent to the right output; a value of +50% or
				more indicates the note is sent entirely to the right and not sent to the left. A value of
				zero places the signal centered between left and right. For example, a value of -250
				indicates that the signal is sent at 75% of full level to the left output and 25% of full
				level to the right output. */
				pan = 17,

				/*Unused, reserved. Should be ignored if encountered. */
				unused2 = 18,

				/*Unused, reserved. Should be ignored if encountered. */
				unused3 = 19,

				/*Unused, reserved. Should be ignored if encountered. */
				unused4 = 20,

				/*This is the delay time, in absolute timecents, from key on until the Modulation LFO
				begins its upward ramp from zero value. A value of 0 indicates a 1 second delay. A
				negative value indicates a delay less than one second and a positive value a delay
				longer than one second. The most negative number (-32768) conventionally
				indicates no delay. For example, a delay of 10 msec would be 1200log2(.01) = -
				7973. */
				delayModLFO = 21,

				/*This is the frequency, in absolute cents, of the Modulation LFOs triangular period.
				A value of zero indicates a frequency of 8.176 Hz. A negative value indicates a
				frequency less than 8.176 Hz; a positive value a frequency greater than 8.176 Hz.
				For example, a frequency of 10 mHz would be 1200log2(.01/8.176) = -11610. */
				freqModLFO = 22,

				/*This is the delay time, in absolute timecents, from key on until the Vibrato LFO
				begins its upward ramp from zero value. A value of 0 indicates a 1 second delay. A
				negative value indicates a delay less than one second; a positive value a delay longer
				than one second. The most negative number (-32768) conventionally indicates no
				delay. For example, a delay of 10 msec would be 1200log2(.01) = -7973. */
				delayVibLFO = 23,

				/*This is the frequency, in absolute cents, of the Vibrato LFOs triangular period. A
				value of zero indicates a frequency of 8.176 Hz. A negative value indicates a
				frequency less than 8.176 Hz; a positive value a frequency greater than 8.176 Hz.
				For example, a frequency of 10 mHz would be 1200log2(.01/8.176) = -11610. */
				freqVibLFO = 24,

				/*This is the delay time, in absolute timecents, between key on and the start of the
				attack phase of the Modulation envelope. A value of 0 indicates a 1 second delay. A
				negative value indicates a delay less than one second; a positive value a delay longer
				than one second. The most negative number (-32768) conventionally indicates no
				delay. For example, a delay of 10 msec would be 1200log2(.01) = -7973. */
				delayModEnv = 25,

				/*This is the time, in absolute timecents, from the end of the Modulation Envelope
				Delay Time until the point at which the Modulation Envelope value reaches its peak.
				Note that the attack is convex; the curve is nominally such that when applied to a
				decibel or semitone parameter, the result is linear in amplitude or Hz respectively. A
				value of 0 indicates a 1 second attack time. A negative value indicates a time less
				than one second; a positive value a time longer than one second. The most negative
				number (-32768) conventionally indicates instantaneous attack. For example, an
				attack time of 10 msec would be 1200log2(.01) = -7973. */
				attackModEnv = 26,

				/*This is the time, in absolute timecents, from the end of the attack phase to the entry
				into decay phase, during which the envelope value is held at its peak. A value of 0
				indicates a 1 second hold time. A negative value indicates a time less than one
				second; a positive value a time longer than one second. The most negative number (-
				32768) conventionally indicates no hold phase. For example, a hold time of 10 msec
				would be 1200log2(.01) = -7973. */
				holdModEnv = 27,

				/*This is the time, in absolute timecents, for a 100% change in the Modulation
				Envelope value during decay phase. For the Modulation Envelope, the decay phase
				linearly ramps toward the sustain level. If the sustain level were zero, the
				Modulation Envelope Decay Time would be the time spent in decay phase. A value
				of 0 indicates a 1 second decay time for a zero-sustain level. A negative value
				indicates a time less than one second; a positive value a time longer than one second.
				For example, a decay time of 10 msec would be 1200log2(.01) = -7973. */
				decayModEnv = 28,

				/*This is the decrease in level, expressed in 0.1% units, to which the Modulation
				Envelope value ramps during the decay phase. For the Modulation Envelope, the
				sustain level is properly expressed in percent of full scale. Because the volume
				envelope sustain level is expressed as an attenuation from full scale, the sustain level
				is analogously expressed as a decrease from full scale. A value of 0 indicates the
				sustain level is full level; this implies a zero duration of decay phase regardless of
				decay time. A positive value indicates a decay to the corresponding level. Values
				less than zero are to be interpreted as zero; values above 1000 are to be interpreted as
				1000. For example, a sustain level which corresponds to an absolute value 40% of
				peak would be 600. */
				sustainModEnv = 29,

				/*This is the time, in absolute timecents, for a 100% change in the Modulation
				Envelope value during release phase. For the Modulation Envelope, the release
				phase linearly ramps toward zero from the current level. If the current level were
				full scale, the Modulation Envelope Release Time would be the time spent in release
				phase until zero value were reached. A value of 0 indicates a 1 second decay time 
				for a release from full level. A negative value indicates a time less than one second;
				a positive value a time longer than one second. For example, a release time of 10
				msec would be 1200log2(.01) = -7973. */
				releaseModEnv = 30,

				/*This is the degree, in timecents per KeyNumber units, to which the hold time of the
				Modulation Envelope is decreased by increasing MIDI key number. The hold time
				at key number 60 is always unchanged. The unit scaling is such that a value of 100
				provides a hold time which tracks the keyboard; that is, an upward octave causes the
				hold time to halve. For example, if the Modulation Envelope Hold Time were -7973
				= 10 msec and the Key Number to Mod Env Hold were 50 when key number 36 was
				played, the hold time would be 20 msec. */
				keynumToModEnvHold = 31,

				/*This is the degree, in timecents per KeyNumber units, to which the hold time of the
				Modulation Envelope is decreased by increasing MIDI key number. The hold time
				at key number 60 is always unchanged. The unit scaling is such that a value of 100
				provides a hold time that tracks the keyboard; that is, an upward octave causes the
				hold time to halve. For example, if the Modulation Envelope Hold Time were -7973
				= 10 msec and the Key Number to Mod Env Hold were 50 when key number 36 was
				played, the hold time would be 20 msec. */
				keynumToModEnvDecay = 32,

				/*This is the delay time, in absolute timecents, between key on and the start of the
				attack phase of the Volume envelope. A value of 0 indicates a 1 second delay. A
				negative value indicates a delay less than one second; a positive value a delay longer
				than one second. The most negative number (-32768) conventionally indicates no
				delay. For example, a delay of 10 msec would be 1200log2(.01) = -7973. */
				delayVolEnv = 33,

				/*This is the time, in absolute timecents, from the end of the Volume Envelope Delay
				Time until the point at which the Volume Envelope value reaches its peak. Note that
				the attack is convex; the curve is nominally such that when applied to the decibel
				volume parameter, the result is linear in amplitude. A value of 0 indicates a 1 second
				attack time. A negative value indicates a time less than one second; a positive value
				a time longer than one second. The most negative number (-32768) conventionally
				indicates instantaneous attack. For example, an attack time of 10 msec would be
				1200log2(.01) = -7973. */
				attackVolEnv = 34,

				/*This is the time, in absolute timecents, from the end of the attack phase to the entry
				into decay phase, during which the Volume envelope value is held at its peak. A
				value of 0 indicates a 1 second hold time. A negative value indicates a time less than
				one second; a positive value a time longer than one second. The most negative
				number (-32768) conventionally indicates no hold phase. For example, a hold time
				of 10 msec would be 1200log2(.01) = -7973. */
				holdVolEnv = 35,

				/*This is the time, in absolute timecents, for a 100% change in the Volume Envelope
				value during decay phase. For the Volume Envelope, the decay phase linearly ramps
				toward the sustain level, causing a constant dB change for each time unit. If the
				sustain level were -100dB, the Volume Envelope Decay Time would be the time
				spent in decay phase. A value of 0 indicates a 1-second decay time for a zero-sustain
				level. A negative value indicates a time less than one second; a positive value a time
				longer than one second. For example, a decay time of 10 msec would be
				1200log2(.01) = -7973. */
				decayVolEnv = 36,

				/*This is the decrease in level, expressed in centibels, to which the Volume Envelope
				value ramps during the decay phase. For the Volume Envelope, the sustain level is
				best expressed in centibels of attenuation from full scale. A value of 0 indicates the
				sustain level is full level; this implies a zero duration of decay phase regardless of
				decay time. A positive value indicates a decay to the corresponding level. Values
				less than zero are to be interpreted as zero; conventionally 1000 indicates full 
				attenuation. For example, a sustain level which corresponds to an absolute value
				12dB below of peak would be 120. */
				sustainVolEnv = 37,

				/*This is the time, in absolute timecents, for a 100% change in the Volume Envelope
				value during release phase. For the Volume Envelope, the release phase linearly
				ramps toward zero from the current level, causing a constant dB change for each
				time unit. If the current level were full scale, the Volume Envelope Release Time
				would be the time spent in release phase until 100dB attenuation were reached. A
				value of 0 indicates a 1-second decay time for a release from full level. A negative
				value indicates a time less than one second; a positive value a time longer than one
				second. For example, a release time of 10 msec would be 1200log2(.01) = -7973. */
				releaseVolEnv = 38,

				/*This is the degree, in timecents per KeyNumber units, to which the hold time of the
				Volume Envelope is decreased by increasing MIDI key number. The hold time at
				key number 60 is always unchanged. The unit scaling is such that a value of 100
				provides a hold time which tracks the keyboard; that is, an upward octave causes the
				hold time to halve. For example, if the Volume Envelope Hold Time were -7973 =
				10 msec and the Key Number to Vol Env Hold were 50 when key number 36 was
				played, the hold time would be 20 msec. */
				keynumToVolEnvHold = 39,

				/*This is the degree, in timecents per KeyNumber units, to which the hold time of the
				Volume Envelope is decreased by increasing MIDI key number. The hold time at
				key number 60 is always unchanged. The unit scaling is such that a value of 100
				provides a hold time that tracks the keyboard; that is, an upward octave causes the
				hold time to halve. For example, if the Volume Envelope Hold Time were -7973 =
				10 msec and the Key Number to Vol Env Hold were 50 when key number 36 was
				played, the hold time would be 20 msec. */
				keynumToVolEnvDecay = 40,

				/*This is the index into the INST sub-chunk providing the instrument to be used for the
				current preset zone. A value of zero indicates the first instrument in the list. The
				value should never exceed two less than the size of the instrument list. The
				instrument enumerator is the terminal generator for PGEN zones. As such, it should
				only appear in the PGEN sub-chunk, and it must appear as the last generator
				enumerator in all but the global preset zone. */
				instrument = 41,

				/*Unused, reserved. Should be ignored if encountered. */
				reserved1 = 42,

				/*This is the minimum and maximum MIDI key number values for which this preset
				zone or instrument zone is active. The LS byte indicates the highest and the MS byte
				the lowest valid key. The keyRange enumerator is optional, but when it does appear,
				it must be the first generator in the zone generator list. */
				keyRange = 43,

				/*This is the minimum and maximum MIDI velocity values for which this preset zone
				or instrument zone is active. The LS byte indicates the highest and the MS byte the
				lowest valid velocity. The velRange enumerator is optional, but when it does appear,
				it must be preceded only by keyRange in the zone generator list. */
				velRange = 44,

				/*The offset, in 32768 sample data point increments beyond the Startloop sample
				header parameter and the first sample data point to be repeated in this instruments
				loop. This parameter is added to the startloopAddrsOffset parameter. For example,
				if Startloop were 5, startloopAddrsOffset were 3 and startAddrsCoarseOffset were 2,
				the first sample data point in the loop would be sample data point 65544. */
				startloopAddrsCoarseOffset = 45,

				/*This enumerator forces the MIDI key number to effectively be interpreted as the
				value given. This generator can only appear at the instrument level. Valid values are
				from 0 to 127. */
				keynum = 46,

				/*This enumerator forces the MIDI velocity to effectively be interpreted as the value
				given. This generator can only appear at the instrument level. Valid values are from
				0 to 127. */
				velocity = 47,

				/*This is the attenuation, in centibels, by which a note is attenuated below full scale. A
				value of zero indicates no attenuation; the note will be played at full scale. For
				example, a value of 60 indicates the note will be played at 6 dB below full scale for
				the note. */
				initialAttenuation = 48,

				/*Unused, reserved. Should be ignored if encountered. */
				reserved2 = 49,

				/*The offset, in 32768 sample data point increments beyond the Endloop sample
				header parameter to the sample data point considered equivalent to the Startloop
				sample data point for the loop for this instrument. This parameter is added to the
				endloopAddrsOffset parameter. For example, if Endloop were 5,
				endloopAddrsOffset were 3 and endAddrsCoarseOffset were 2, sample data point
				65544 would be considered equivalent to the Startloop sample data point, and hence
				sample data point 65543 would effectively precede Startloop during looping. */
				endloopAddrsCoarseOffset = 50,

				/*This is a pitch offset, in semitones, which should be applied to the note. A positive
				value indicates the sound is reproduced at a higher pitch; a negative value indicates a
				lower pitch. For example, a Coarse Tune value of -4 would cause the sound to be
				reproduced four semitones flat. */
				coarseTune = 51,

				/*This is a pitch offset, in cents, which should be applied to the note. It is additive
				with coarseTune. A positive value indicates the sound is reproduced at a higher
				pitch; a negative value indicates a lower pitch. For example, a Fine Tuning value of -
				5 would cause the sound to be reproduced five cents flat. */
				fineTune = 52,

				/*This is the index into the SHDR sub-chunk providing the sample to be used for the
				current instrument zone. A value of zero indicates the first sample in the list. The
				value should never exceed two less than the size of the sample list. The sampleID
				enumerator is the terminal generator for IGEN zones. As such, it should only appear
				in the IGEN sub-chunk, and it must appear as the last generator enumerator in all but
				the global zone. */
				sampleID = 53,

				/*This enumerator indicates a value which gives a variety of Boolean flags describing
				the sample for the current instrument zone. The sampleModes should only appear in
				the IGEN sub-chunk, and should not appear in the global zone. The two LS bits of
				the value indicate the type of loop in the sample: 0 indicates a sound reproduced with
				no loop, 1 indicates a sound which loops continuously, 2 is unused but should be
				interpreted as indicating no loop, and 3 indicates a sound which loops for the
				duration of key depression then proceeds to play the remainder of the sample. */
				sampleModes = 54,

				/*Unused, reserved. Should be ignored if encountered. */
				reserved3 = 55,

				/*This parameter represents the degree to which MIDI key number influences pitch. A
				value of zero indicates that MIDI key number has no effect on pitch; a value of 100
				represents the usual tempered semitone scale. */
				scaleTuning = 56,

				/*This parameter provides the capability for a key depression in a given instrument to
				terminate the playback of other instruments. This is particularly useful for
				percussive instruments such as a hi-hat cymbal. An exclusive class value of zero
				indicates no exclusive class; no special action is taken. Any other value indicates
				that when this note is initiated, any other sounding note with the same exclusive class
				value should be rapidly terminated. The exclusive class generator can only appear at
				the instrument level. The scope of the exclusive class is the entire preset. In other 
				words, any other instrument zone within the same preset holding a corresponding
				exclusive class will be terminated. */
				exclusiveClass = 57,

				/*This parameter represents the MIDI key number at which the sample is to be played
				back at its original sample rate. If not present, or if present with a value of -1, then
				the sample header parameter Original Key is used in its place. If it is present in the
				range 0-127, then the indicated key number will cause the sample to be played back
				at its sample header Sample Rate. For example, if the sample were a recording of a
				piano middle C (Original Key = 60) at a sample rate of 22.050 kHz, and Root Key
				were set to 69, then playing MIDI key number 69 (A above middle C) would cause a
				piano note of pitch middle C to be heard.*/
				overridingRootKey = 58,

				/*Unused, reserved. Should be ignored if encountered. */
				unused5 = 59,

				/*Unused, reserved. Should be ignored if encountered. Unique name provides value
				to end of defined list. */
				endOper = 60
			};
			GenType enumeration;

			friend inline bool operator==(const SFGenerator& lhs, const SFGenerator& rhs)
			{
				return lhs.enumeration == rhs.enumeration;
			}
			friend inline bool operator!=(const SFGenerator& lhs, const SFGenerator& rhs)
			{
				return !(lhs == rhs);
			}
			friend inline bool operator==(const SFGenerator& lhs, const GenType& rhs)
			{
				return lhs.enumeration == rhs;
			}
			friend inline bool operator!=(const SFGenerator& lhs, const GenType& rhs)
			{
				return !(lhs == rhs);
			}
		};
		struct SFTransform
		{
			WORD enumeration;
		};

		struct sfVersionTag
		{
			WORD wMajor;
			WORD wMinor;
		};

		RIFF::stream* stream;

		sfVersionTag ifil;
		sfVersionTag iver;
		std::string szSoundEngine;
		std::string szROM;
		std::string szName;
		std::string szDate;
		std::string szProduct;
		std::string szCreator;
		std::string szCopyright;
		std::string szComment;
		std::string szTools;

		struct HYDRA
		{
			struct sfPresetHeader
			{
				CHAR achPresetName[20];
				WORD wPreset;//MIDI Preset Number
				WORD wBank;//MIDI Bank Number
				WORD wPresetBagNdx;//index to the presets zone list
				DWORD dwLibrary;//reserved
				DWORD dwGenre;//reserved
				DWORD dwMorphology;//reserved
			};
			//Preset header list
			std::vector<sfPresetHeader*> phdr;

			struct sfPresetBag
			{
				WORD wGenNdx;//index to the presets zone list of generators in PGEN
				WORD wModNdx;//index to the list of modulators in PMOD
			};
			//Pointers to first entries of preset zone generator and modulator lists
			std::vector<sfPresetBag*> pbag;

			struct sfModList
			{
				//indicates the source of data for the modulator
				SFModulator sfModSrcOper;
				//destination of the modulator
				SFGenerator sfModDestOper;
				//degree to which the source modulates the destination
				//a zero value indicates there is no fixed amount. 
				SHORT modAmount;
				//indicates the degree to which the source modulates the destination
				//is to be controlled by the specified modulation source
				SFModulator sfModAmtSrcOper;
				//indicates that a transform of the specified type will be applied
				//to the modulation source before application to the modulator
				SFTransform sfModTransOper;
			};
			//Preset zone modulators
			std::vector<sfModList*> pmod;

			struct sfGenList
			{
				SFGenerator sfGenOper;
				//value to be assigned to the specified generator
				//note that this can be of three formats. 
				genAmountType genAmount;
			};
			//Preset zone generators
			std::vector<sfGenList*> pgen;

			struct sfInst
			{
				CHAR achInstName[20];
				//index to the instruments zone list in the IBAG sub-chunk
				WORD wInstBagNdx;
			};
			//Instrument list
			std::vector<sfInst*> inst;

			struct sfInstBag
			{
				//index to the instrument zones list of generators in the IGEN sub-chunk
				WORD wInstGenNdx;
				//index to the list of modulators in the IMOD sub-chunk
				WORD wInstModNdx;
			};
			//Pointers to first entries of instrument zone generator and modulator lists
			std::vector<sfInstBag*> ibag;

			struct sfInstModList
			{
				//indicates the source of data for the modulator
				SFModulator sfModSrcOper;
				//indicates the destination of the modulator
				SFGenerator sfModDestOper;
				//indicates the degree to which the source modulates the destination
				//a zero value indicates there is no fixed amount. 
				SHORT modAmount;
				//indicates the degree to which the source modulates the destination
				//is to be controlled by the specified modulation source
				SFModulator sfModAmtSrcOper;
				//indicates that a transform of the specified type will be applied
				//to the modulation source before application to the modulator
				SFTransform sfModTransOper;
			};
			//Instrument zone modulators
			std::vector<sfInstModList*> imod;

			struct sfInstGenList
			{
				SFGenerator sfGenOper;
				//value to be assigned to the specified generator
				//note that this can be of three formats.
				genAmountType genAmount;
			};
			//Instrument zone generators
			std::vector<sfInstGenList*> igen;

			struct sfSample
			{
				CHAR achSampleName[20];
				//index, in sample data points, from the beginning of the sample data
				//field to the first data point of this sample. 
				DWORD dwStart;
				//index, in sample data points, from the beginning of the sample data
				//field to the first of the set of 46 zero valued data points following
				//this sample. 
				DWORD dwEnd;
				//index, in sample data points, from the beginning of the sample data
				//field to the first data point in the loop of this sample. 
				DWORD dwStartloop;
				//index, in sample data points, from the beginning of the sample data
				//field to the first data point following the loop of this sample.
				//Note that this is the data point equivalent to the first loop data point,
				//and that to produce portable artifact free loops, the eight proximal data
				//points surrounding both the Startloop and Endloop points should be identical. 
				DWORD dwEndloop;
				//the sample rate, in hertz, at which this sample was acquired or to which
				//it was most recently converted.
				DWORD dwSampleRate;
				//contains the MIDI key number of the recorded pitch of the sample
				BYTE byOriginalKey;
				//contains a pitch correction in cents that should be applied to
				//the sample on playback.
				//The purpose of this field is to compensate for any pitch errors during
				//the sample recording process. The correction value is that of the correction
				//to be applied. For example, if the sound is 4 cents sharp, a correction
				//bringing it 4 cents flat is required; thus the value should be -4.
				CHAR chCorrection;
				//sample header link, accordingly indicated by sfSampleType
				WORD wSampleLink;
				//sample type
				SFSampleLink sfSampleType;
			};
			//Samples
			std::vector<sfSample*> shdr;

			~HYDRA()
			{
				for(auto i : ibag) delete i;
				for(auto i : igen) delete i;
				for(auto i : imod) delete i;
				for(auto i : inst) delete i;
				for(auto i : pbag) delete i;
				for(auto i : pgen) delete i;
				for(auto i : phdr) delete i;
				for(auto i : pmod) delete i;
				for(auto i : shdr) delete i;
			}
		};
		HYDRA hydra;

		size_t sample_data_offset;
		size_t sample_data_24_offset;

		struct Sample
		{
			std::string name;

			uint32_t loop_start;
			uint32_t loop_end;

			uint32_t sample_rate;
			uint8_t original_key;
			int8_t correction;

			uint32_t data_stream_offset;
			float* data = nullptr;
			uint32_t size;

			SFSampleLink sample_type;
			Sample* linked_sample;

			~Sample()
			{
				delete data;
			}

			void load_data(SoundFont2& sf2)
			{
				SF2_DEBUG_OUTPUT((std::string("Loading sample data \"") + name + "\"...\n").c_str());
				int16_t* data16 = new int16_t[size];
				//set up position of the stream
				sf2.stream->setpos(sf2.sample_data_offset+data_stream_offset*sizeof(int16_t));
				//read 16 bit samples
				sf2.stream->read(data16, size*sizeof(int16_t));
				if(sf2.sample_data_24_offset)
				{
					uint8_t* data24 = new uint8_t[size];
					//set up position of the stream
					sf2.stream->setpos(sf2.sample_data_24_offset+data_stream_offset);
					//read 8 bit of 24 bit complementary additional sample data
					sf2.stream->read(data24, size);
					//combine both buffers, convert to float and store
					data = new float[size];
					for(uint32_t j = 0; j < size; ++j)
					{
						data[j] = (float)
							(
								(
									(((uint8_t*)&data16[j])[1] << 24) |
									(((uint8_t*)&data16[j])[0] << 16) |
									(data24[j] << 8)
									) >> 8
								) / 8388607.0;
					}
					delete data16;
					delete data24;
				}
				else
				{
					//just convert to float and store
					data = new float[size];
					for(uint32_t j = 0; j < size; ++j)
					{
						data[j] = (float)data16[j] / 32767.0;
					}
					delete data16;
				}

				//test
				//save RAW
				/*std::ofstream outfile ("wave.raw",std::ofstream::binary);
				outfile.write((char*)data, sizeof(float)*size);
				outfile.close();*/
			}
		};
		std::vector<Sample*> samples;

		struct LFO
		{
			int16_t delay = -12000;
			int16_t frequency = 0;

			LFO& operator+=(const LFO& rhs)
			{
				delay += rhs.delay;
				frequency += rhs.frequency;
				return *this;
			}

			friend LFO operator+(LFO lhs, const LFO& rhs)
			{
				lhs += rhs;
				return lhs;
			}
		};
		struct Envelope
		{
			int16_t attack = 0;
			int16_t decay = 0;
			int16_t sustain = 0;
			int16_t release = 0;
			int16_t hold = 0;
			int16_t delay = 0;

			int16_t keynumToHold = 0;
			int16_t keynumToDecay = 0;

			void SetToDefault()
			{
				attack = -12000;
				decay = -12000;
				release = -12000;
				hold = -12000;
				delay = -12000;
			}

			Envelope& operator+=(const Envelope& rhs)
			{
				attack += rhs.attack;
				decay += rhs.decay;
				sustain += rhs.sustain;
				release += rhs.release;
				hold += rhs.hold;
				delay += rhs.delay;
				keynumToHold += rhs.keynumToHold;
				keynumToDecay += rhs.keynumToDecay;
				return *this;
			}

			friend Envelope operator+(Envelope lhs, const Envelope& rhs)
			{
				lhs += rhs;
				return lhs;
			}
		};
		enum class LoopMode
		{
			None = 0,
			Continuous = 1,
			Sustain = 2
		};

		struct Instrument
		{
			std::string name;

			//Also known as Split
			//used for different key and velocity ranges
			struct Zone
			{
				Sample* sample = nullptr;
				int32_t start_offset = 0;
				int32_t end_offset = 0;
				int32_t loop_start_offset = 0;
				int32_t loop_end_offset = 0;
				int16_t filter_freq = 13500;
				float filter_q = 0.0f;
				int16_t chorus_send = 0;
				int16_t reverb_send = 0;
				float scale_tuning = 1.0f;
				int16_t root_key = -1;
				uint8_t key_low = 0, key_high = 127;
				uint8_t vel_low = 0, vel_high = 127;
				int16_t keynum = -1;
				int16_t velocity = -1;
				int32_t tune = 0;
				uint16_t exclusive_class = 0;
				float pan = 0.0f;
				float attenuation = 0.0f;
				LoopMode loop_mode = LoopMode::None;

				LFO modLFO;
				int16_t modLFO_to_pitch = 0;
				int16_t modLFO_to_filter_fc = 0;
				int16_t modLFO_to_volume = 0;
				LFO vibLFO;
				int16_t vibLFO_to_pitch = 0;

				Envelope modEnv;
				int16_t modEnv_to_pitch = 0;
				int16_t modEnv_to_filter_fc = 0;
				Envelope volEnv;
			};
			//Zone* global_zone = nullptr;
			std::vector<Zone*> splits;
		};
		std::vector<Instrument*> instruments;

		struct Preset
		{
			std::string name;
			//MIDI Preset Number
			uint16_t num;

			//Also known as Layer
			//used to specify list of layered instruments of a preset
			//does not contain default generators
			struct Zone
			{
				Instrument* instrument = nullptr;
				int16_t filter_freq = 0;
				float filter_q = 0.0f;
				int16_t chorus_send = 0;
				int16_t reverb_send = 0;
				float scale_tuning = 0.0f;
				uint8_t key_low = 0, key_high = 127;
				uint8_t vel_low = 0, vel_high = 127;
				int32_t tune = 0;
				float pan = 0.0f;
				float attenuation = 0.0f;

				LFO modLFO = {0,0};
				int16_t modLFO_to_pitch = 0;
				int16_t modLFO_to_filter_fc = 0;
				int16_t modLFO_to_volume = 0;
				LFO vibLFO = {0,0};
				int16_t vibLFO_to_pitch = 0;

				Envelope modEnv;
				int16_t modEnv_to_pitch = 0;
				int16_t modEnv_to_filter_fc = 0;
				Envelope volEnv;
			};
			//Zone* global_zone = nullptr;
			std::vector<Zone*> layers;
		};
		struct Bank
		{
			uint16_t num;//MIDI Bank Number
			std::vector<Preset*> presets;
		};
		std::vector<Bank*> banks;

		struct BiQuadLowpass
		{
			float inv_Q;
			float a0;
			float a1;
			float b1;
			float b2;
			float z1 = 0;
			float z2 = 0;
			bool active;

			inline void set_Q(float Q)
			{
				inv_Q = 1.0f / Q;
			}

			inline void set_frequency(float Fc)
			{
				float K = std::tan(M_PI * Fc);
				float KK = K * K;
				float norm = 1.0f / (1.0f + K * inv_Q + KK);
				a0 = KK * norm;
				a1 = 2.0f * a0;
				b1 = 2.0f * (KK - 1) * norm;
				b2 = (1.0f - K * inv_Q + KK) * norm;
			}

			inline float process(float in)
			{
				float out = in * a0 + z1;
				z1 = in * a1 + z2 - b1 * out;
				z2 = in * a0 - b2 * out;
				return out;
			}
		};

		struct Voice
		{
			Instrument::Zone* zone = nullptr;

			Sample* sample = nullptr;
			double sample_pos = 0;
			uint32_t sample_end_pos = 0;

			bool hold = false;
			uint32_t loop_start = 0;
			uint32_t loop_end = 0;

			float pan_factor_l = 0.0f, pan_factor_r = 0.0f;
			//float src_freq_factor = 0.0f;
			float freq = 0.0f;
			float gain = 0.0f;
			float filter_freq = 0.0f;
			float filter_q = 0.0f;
			float modenv_to_filter_freq = 0.0f;
			float modenv_to_pitch = 0.0f;

			uint8_t key = 0;

			enum class EnvPhase : int
			{
				Delay = 0,
				Attack = 1,
				Hold = 2,
				Decay = 3,
				Sustain = 4,
				Release = 5,
				End = 6
			};

			template <bool is_decibels = true>
			struct Env
			{
				float
					delay,
					attack,
					hold,
					decay,
					sustain,
					release;

				float keynumToHold = 1.0f;
				float keynumToDecay = 1.0f;

				double slope_factor;
				EnvPhase phase = EnvPhase::Delay;
				double time = 0;
				float value = 0;

				Env()
				{

				}
				Env(const Envelope& env, const Envelope& env2, uint8_t key)
				{
					if constexpr(is_decibels) value = -96.0f;

					Envelope tmp_envelope = env+env2;
					//setup volume envelope
					delay = timecents_to_seconds(tmp_envelope.delay);
					attack = timecents_to_seconds(tmp_envelope.attack);
					hold = timecents_to_seconds(tmp_envelope.hold);
					hold *= timecents_to_seconds(tmp_envelope.keynumToHold*(60-key));
					decay = timecents_to_seconds(tmp_envelope.decay);
					decay *= timecents_to_seconds(tmp_envelope.keynumToDecay*(60-key));
					release = timecents_to_seconds(tmp_envelope.release);
					if constexpr(is_decibels)
						sustain = tmp_envelope.sustain*0.1f;//this one is decibels
					else
						sustain = 1.0f-tmp_envelope.sustain*0.001f;//this one is 0.1 units, expressed as percents
					slope_factor = 1.0f/delay;
				}

				//ADSR envelope operates in decibels
				float Get(float delta_time)
				{
					switch(phase)
					{
					case EnvPhase::Delay:
					{
						//wait before fading in
						time += delta_time;
						if(time >= delay)
						{
							//go to the next phase
							time -= delay;
							phase = EnvPhase::Attack;
							slope_factor = 1.0f/attack;				
						}
						if constexpr(is_decibels)
							return -96.0f;
						else
							return 0.0f;
						break;
					}
					case EnvPhase::Attack:
					{
						float val;
						if constexpr(is_decibels)
							val = gain_to_decibels(time * slope_factor);
						else
							val = time * slope_factor;
						value = val;

						time += delta_time;
						if(time >= attack)
						{
							//go to the next phase
							time -= attack;
							phase = EnvPhase::Hold;
							slope_factor = 1.0f/hold;
						}
						return val;
					}
					case EnvPhase::Hold:
					{
						//stay at maximum level for some time
						if constexpr(is_decibels)
							value = 0.0f;
						else
							value = 1.0f;

						time += delta_time;
						if(time >= hold)
						{
							//go to the next phase
							time -= hold;
							phase = EnvPhase::Decay;
							slope_factor = 1.0f/decay;
						}
						if constexpr(is_decibels)
							return 0.0f;
						else
							return 1.0f;
					}
					case EnvPhase::Decay:
					{
						//fade from maximum to sustain level
						float val;
						if constexpr(is_decibels)
							val = -sustain * time * slope_factor;
						else
							val = fast_lerp(1.0f, sustain, time * slope_factor);
						value = val;
						time += delta_time;
						if(time >= decay)
						{
							//go to the next phase
							time -= decay;
							phase = EnvPhase::Sustain;
						}
						return value;
					}
					case EnvPhase::Sustain:
					{
						//wait for note off
						if constexpr(is_decibels)
							return -sustain;
						else
							return sustain;
					}
					case EnvPhase::Release:
					{
						//fade out linearly
						float val;
						if constexpr(is_decibels)
							val = value + (time * slope_factor) * (-96.0f - value);
						else
							val = fast_lerp(value, 0.0f, time * slope_factor);
						time += delta_time;
						if(time >= release)
						{
							//go to the next phase
							time -= release;
							phase = EnvPhase::End;
						}
						return val;
					}
					default:
					{
						//finished or error
						if constexpr(is_decibels)
							return -96.0f;
						else
							return 0.0f;
					}
					}

					//unreachable code
					/*if constexpr(is_decibels)
						return -96.0f;
					else
						return 0.0f;*/
				}

				void Release()
				{
					slope_factor = 1.0f/release;
					phase = Voice::EnvPhase::Release;
					time = 0.0f;
				}
			};
			Env<true> volenv;
			Env<false> modenv;
			BiQuadLowpass lowpass;

			struct VoiceLFO
			{
				float time;
				float freq;
				float delay;

				VoiceLFO()
				{
				}

				VoiceLFO(LFO& lfo, LFO& lfo2)
				{
					LFO tmp_lfo = lfo+lfo2;
					time = 0.0f;
					freq = 8.176f*cents_to_hertz(tmp_lfo.frequency);
					delay = timecents_to_seconds(tmp_lfo.delay);
				}

				float Get(float delta_time)
				{
					//triangle wave
					time += delta_time;
					return (time < delay)?0.0f:abs(fmod(4.0f*freq*(time-delay)+3.0f, 4.0f)-2.0f)-1.0f;
				}
			};
			VoiceLFO modLFO;
			float modLFO_to_pitch = 0;
			float modLFO_to_filter_fc = 0;
			float modLFO_to_volume = 0;
			VoiceLFO vibLFO;
			float vibLFO_to_pitch = 0;

			//Note Off
			void Release()
			{
				hold = false;
			}

			void Render(float* output_L, float* output_R, uint32_t size, float sample_rate)
			{
				//Wavetable oscillator implementation
				//TODO: optimize

				//inverse of sample rate (which is also delta time)
				double sample_rate_inv = 1.0f/sample_rate;

				//calculate sample data playback step
				//float step = sample->sample_rate/(source_frequency*sample_rate)*frequency;
				//double step = (src_freq_factor*freq)/sample_rate;
				double step = freq/sample_rate;
				double step_ = step;
				//time, in seconds, specifying duration of a sample
				double delta_time = sample_rate_inv;
				for(uint32_t i = 0; i < size && sample_pos < sample->size; ++i)
				{
					//truncate floating point position to integer
					//effectively removing fractional part
					uint32_t pos = sample_pos;
					bool is_looping = ((hold && zone->loop_mode != LoopMode::None) || zone->loop_mode == LoopMode::Continuous);
					//get position next to the current or wrapped around if out of loop bounds
					uint32_t pos_next = (pos >= loop_end && is_looping || pos + 1 >= sample->size)?((is_looping)?loop_start:pos):(pos + 1);
					//get interpolation factor by subtracting truncated position from original,
					//resulting value is the fractional part of the position
					float lerp_factor = sample_pos - float(pos);
					//get interpolated value between two adjacent sample points
					float val = fast_lerp(sample->data[pos], sample->data[pos_next], lerp_factor);
					if(pos > sample->size) printf("pos out of range!\n");
					if(pos_next > sample->size) printf("pos_next out of range!\n");
					if(sample_pos >= sample->size)
					{
						printf("out of range: %f\n", val);
					}
					//increment sample position using pitch based step value
					sample_pos += step;
					if(sample_pos >= loop_end && is_looping)
						sample_pos -= (loop_end - loop_start);

					auto tmp_state = volenv.phase;
					float volenv_gain = decibels_to_gain(volenv.Get(delta_time));
					float modenv_gain = modenv.Get(delta_time);
					//prematurely end inaudible voice (optimization)
					//TODO: add an option to disable
					if(volenv_gain < 0.002f && volenv.phase == Voice::EnvPhase::Release)
						volenv.phase = Voice::EnvPhase::End;
					//release volume envelope
					if(static_cast<int>(volenv.phase) < static_cast<int>(Voice::EnvPhase::Release) && !hold)
					{
						//immediately jump to the release phase after a note off
						volenv.Release();
					}
					//release modulation envelope
					if(static_cast<int>(modenv.phase) < static_cast<int>(Voice::EnvPhase::Release) && !hold)
					{
						//immediately jump to the release phase after a note off
						modenv.Release();
					}
					val *= gain * volenv_gain;
					float viblfo_val;
					if(vibLFO_to_pitch)
						viblfo_val = vibLFO.Get(delta_time);
					float modlfo_val;
					if(modLFO_to_pitch || modLFO_to_filter_fc || modLFO_to_volume)
						modlfo_val = modLFO.Get(delta_time);
					float filter_freq_new = filter_freq;
					if(modenv_to_filter_freq || modLFO_to_filter_fc)
					{
						if(modenv_to_filter_freq)
							filter_freq_new *= cents_to_hertz(modenv_gain*modenv_to_filter_freq);
						if(modLFO_to_filter_fc)
							filter_freq_new *= cents_to_hertz(modlfo_val*modLFO_to_filter_fc);
						//lowpass.active = !(filter_freq_new > sample_rate*0.45f || filter_q == 0.0f);
					}
					if(lowpass.active)
					{			
						//Spec says 20kHz is the limit but it causes clicks,
						//so a multiple of sample rate is used instead, to avoid biquad filter instability
						//I don't exclude the possibility that maybe I misunderstood something though
						/*if(filter_freq_new > sample_rate * 0.45f)
						{
							filter_freq_new = sample_rate*0.45f;
						}*/if(filter_freq_new > sample_rate * 0.4977f)
						{
							filter_freq_new = sample_rate*0.4977f;
						}
						if(filter_freq_new != filter_freq)
						{
							//lowpass.set_frequency(filter_freq_new/sample_rate);
							lowpass.set_frequency(filter_freq_new*sample_rate_inv);
						}
						val = lowpass.process(val);
					}
					float adjusted_pitch = 1.0f;
					if(modenv_to_pitch || vibLFO_to_pitch || modLFO_to_pitch)
					{
						if(modenv_to_pitch)
							adjusted_pitch *= cents_to_hertz(modenv_gain*modenv_to_pitch);
						if(vibLFO_to_pitch)
							adjusted_pitch *= cents_to_hertz(vibLFO_to_pitch*viblfo_val);
						if(modLFO_to_pitch)
							adjusted_pitch *= cents_to_hertz(modLFO_to_pitch*modlfo_val);
						//step = (src_freq_factor*freq*adjusted_pitch)/sample_rate;
						step = step_*adjusted_pitch;
					}
					if(modLFO_to_volume)
					{
						val *= decibels_to_gain(modlfo_val*modLFO_to_volume);
					}
					//val = volenv_gain;
					//val = modenv_gain;
					/*if(modLFO_to_pitch || modLFO_to_filter_fc || modLFO_to_volume)
					val = modlfo_val;
					else val = 0;*/

					//add value to the outputs with appropriate panning factors applied
					output_L[i] += val * pan_factor_l;
					output_R[i] += val * pan_factor_r;
				}

				//test
				//save RAW
				/*std::ofstream outfile ("wave.raw",std::ofstream::binary);
				outfile.write((char*)sample->data, sizeof(float)*sample->size);
				outfile.close();*/
			}

			bool IsDone()
			{
				return (sample_pos >= sample->size && !hold) || volenv.phase == Voice::EnvPhase::End;
			}
		};

		struct Channel
		{
			DynamicPool<Voice> voices = DynamicPool<Voice>(64, 64);
			std::vector<bool> key_states;
			Bank* bank = nullptr;
			Preset* preset = nullptr;
			SoundFont2* sf = nullptr;
			bool sustain = false;

			Channel()
			{
				key_states.resize(255, false);
			}

			~Channel()
			{
				//for(auto v : voices) delete v;
			}

			void SetPreset(size_t presetno, size_t bankno = 0)
			{
				if(!sf) return;
				if(sf->banks.empty()) return;

				//find bank by number
				Bank* target_bank = nullptr;
				for(auto b : sf->banks)
				{
					if(b->num == bankno)
					{
						target_bank = b;
						break;
					}
				}
				if(!target_bank) goto fallback_bank_zero;

				//try to find a matching preset in requested bank
				for(auto p : target_bank->presets)
				{
					if(p->num == presetno)
					{
						bank = target_bank;
						preset = p;
						goto load_samples_proc;
					}
				}
				//fallback to default bank 0
				//exception: percussion bank 128
				if(target_bank->num == 128)
				{
					if(!target_bank->presets.empty())
					{
						preset = target_bank->presets[0];
						goto load_samples_proc;
					}
				}
			fallback_bank_zero:
				for(auto p : sf->banks[0]->presets)
				{
					if(p->num == presetno)
					{
						bank = sf->banks[0];
						preset = p;
						goto load_samples_proc;
					}
				}

			load_samples_proc:
				//load samples
				if(preset)
				{
					for(auto layer : preset->layers)
					{
						for(auto split : layer->instrument->splits)
						{
							if(!split->sample->data)
							{
								split->sample->load_data(*sf);
							}
						}
					}
					return;
				}
				//failsafe
				//on the second thought.. no failsafe
			}

			void NoteOn(uint8_t key, uint8_t velocity, float sample_rate)
			{
				if(!preset || !sf) return;

				key_states[key] = true;

				size_t size = voices.size();
				sf->GenerateVoices(preset, key, velocity, sample_rate, voices);

				//release exclusive voices
				for(auto nv = voices.begin()+size; nv != voices.end(); ++nv)
				{
					if(nv->zone->exclusive_class != 0)
					{
						for(auto v = voices.begin(); v != voices.begin()+size; ++v)
						{
							if(v->zone->exclusive_class == nv->zone->exclusive_class)
							{
								v->Release();
								//extinguish fast
								v->volenv.value = v->volenv.Get(0.0f);
								v->volenv.time = 0.0f;
								v->volenv.release = 0.001f;
							}
						}
					}
				}
			}

			void NoteOff(uint8_t key)
			{
				key_states[key] = false;
				if(!sustain)
				{
					for(auto& v : voices)
					{
						if(v.key == key)
						{
							v.Release();
						}
					}
				}
			}

			void SetSustain(bool enable)
			{
				sustain = enable;
				if(!enable)
				{
					//go through all recorded key states
					for(size_t key = 0; key < key_states.size(); ++key)
					{
						//check if key is not held
						if(!key_states[key])
						{
							//release all voices with that key
							for(size_t i = 0; i < voices.size(); i++)
							{
								if(voices[i].key == key)
								{
									voices[i].Release();
								}
							}
						}
					}
				}
			}

			void Render(float* output_L, float* output_R, uint32_t size, float sample_rate)
			{
				for(size_t i = 0; i < voices.size();)
				{
					auto& v = voices[i];
					v.Render(output_L, output_R, size, sample_rate);
					if(v.IsDone())
					{
						//delete v;
						voices.erase(voices.begin() + i);
					}
					else ++i;
				}
			}

			void Panic()
			{
				//for(auto v : voices) delete v;
				voices.clear();
			}
		};

		//Note On
		void GenerateVoices(Preset* preset, uint8_t key, uint8_t velocity, float sample_rate, DynamicPool<Voice>& container)
		{
			for(auto layer : preset->layers)
			{
				//check if passes by key and velocity
				if(key < layer->key_low || layer->key_high < key ||
				   velocity < layer->vel_low || layer->vel_high < velocity)
					continue;
				for(auto split : layer->instrument->splits)
				{
					//check if passes by key and velocity
					if(key < split->key_low || split->key_high < key ||
					   velocity < split->vel_low || split->vel_high < velocity)
						continue;
					//check if sample isn't ROM (because it's not supported)
					if(IsSampleROM(split->sample->sample_type)) continue;

					uint8_t tmp_vel = velocity;
					uint8_t tmp_key = key;
					//override velocity
					if(split->velocity != -1)
						tmp_vel = split->velocity;
					//override key
					if(split->keynum != -1)
						tmp_key = split->keynum;

					//get sample
					Sample* sample_first = split->sample;
					Sample* sample = sample_first;
					float pan = 0.0f;//mono

				create_voice:
					{
						//create new voice
						container.emplace_back();
						auto voice = &container.back();
						voice->key = tmp_key;
						voice->sample = sample;
						voice->zone = split;
						voice->hold = true;

						//sample points
						voice->sample_pos = split->start_offset;
						voice->sample_end_pos = sample->size + split->end_offset;

						//loop points
						voice->loop_start = sample->loop_start + split->loop_start_offset;
						voice->loop_end = sample->loop_end + split->loop_end_offset;

						//add preset and instrument envelope value generators together
						voice->volenv = Voice::Env<true>(layer->volEnv, split->volEnv, key);
						voice->modenv = Voice::Env<false>(layer->modEnv, split->modEnv, key);
						//setup lowpass filter
						//8.176f - MIDI key 0 frequency used to convert "absolute pitch cents" to Hz
						voice->filter_q = (layer->filter_q+split->filter_q);
						voice->filter_freq = 8.176f*cents_to_hertz(layer->filter_freq+split->filter_freq);
						voice->modenv_to_filter_freq = layer->modEnv_to_filter_fc+split->modEnv_to_filter_fc;
						voice->lowpass.active = !(voice->filter_freq > 20000.0f && voice->filter_q < 0.0f && voice->modenv_to_filter_freq != 0.0f);
						if(voice->lowpass.active)
						{
							voice->lowpass.set_Q(decibels_to_gain(voice->filter_q));
							voice->lowpass.set_frequency(voice->filter_freq/sample_rate);
						}
						voice->modenv_to_pitch = layer->modEnv_to_pitch+split->modEnv_to_pitch;

						voice->modLFO = Voice::VoiceLFO(layer->modLFO, split->modLFO);
						voice->modLFO_to_filter_fc = layer->modLFO_to_filter_fc + split->modLFO_to_filter_fc;
						voice->modLFO_to_pitch = layer->modLFO_to_pitch + split->modLFO_to_pitch;
						voice->modLFO_to_volume = (layer->modLFO_to_volume + split->modLFO_to_volume)/10.0f;
						voice->vibLFO = Voice::VoiceLFO(layer->modLFO, split->modLFO);
						voice->vibLFO_to_pitch = layer->vibLFO_to_pitch + split->vibLFO_to_pitch;

						//Calculate gain
						//Factor of 0.4 is for compatibility, many soundfonts expect this behaviour...
						//Even though it's against the specification, apparently that's how some
						//E-MU synthesizers are designed as well, which leaves many questions.
						//TODO: add an option to disable this behaviour
						voice->gain = decibels_to_gain(-(layer->attenuation + split->attenuation)*0.4);
						//linear velocity curve
						voice->gain *= float(tmp_vel) / 127.0f;

						//get pan on per sample basis
						switch(sample->sample_type)
						{
						case SFSampleLink::monoSample: pan = 0.0f; break;
						case SFSampleLink::leftSample: pan = -0.5f; break;
						case SFSampleLink::rightSample: pan = 0.5f; break;
						case SFSampleLink::linkedSample: pan = 0.0f; break;
						}
						//calculate panning factors
						constant_power_pan(
							voice->pan_factor_l,
							voice->pan_factor_r,
							clamp_panning(pan + layer->pan + split->pan)
						);

						//printf("pitch correction: %f\n", sample->correction);

						//calculate pitch factors
						float root_key_cents = ((split->root_key == -1)?sample->original_key:split->root_key)*100.0f;
						float note_cents = tmp_key*100.0f + split->tune + layer->tune;
						float src_freq_factor = sample->sample_rate/cents_to_hertz(root_key_cents);
						voice->freq = src_freq_factor*cents_to_hertz(root_key_cents + (note_cents - root_key_cents)*(split->scale_tuning+layer->scale_tuning));
						if(sample->correction)
						{
							voice->freq *= cents_to_hertz(sample->correction);
						}

						//Check for next linked sample if sample type isn't mono.
						//Since SoundFont 2.4 doesn't yet define circular linking,
						//all we have to do is just handle one more sample for stereo
						//but we'll go ahead and try to loop all of them.
						if(split->sample->sample_type != SFSampleLink::monoSample)
						{
							//check for full circle
							if(sample->linked_sample == sample_first || !sample->linked_sample)
							{
								//the circle is now complete
								continue;
							}

							//get next sample
							sample = sample->linked_sample;
							//start over
							goto create_voice;
						}
					}
				}
			}
		}

		SoundFont2(RIFF::RIFF* riff, RIFF::stream* s)
		{
#define read_zstr(chunk, string, max_len)\
			{\
				if(chunk)\
				{\
					s->setpos(chunk->data_offset);\
					for(int i = 0; i < max_len; ++i)\
					{\
						char c = 0;\
						s->read(&c, sizeof(char));\
						if(c != 0)\
							string += c;\
						else break;\
					}\
				}\
			}
#define read_versiontag(chunk, tag)\
			{\
				if(chunk)\
				{\
					s->setpos(chunk->data_offset);\
					s->read(&tag.wMajor, sizeof(WORD));\
					s->read(&tag.wMinor, sizeof(WORD));\
				}\
			}
#define read_field(field){s->read(&field, sizeof(field));}

			RIFF_SoundFont2 sf2(riff);

			SF2_DEBUG_OUTPUT("Reading file info...\n");
			read_versiontag(sf2.INFO.ifil, ifil, 256);
			//check version
			if(ifil.wMajor != 2)
			{
				//not supported
				//TODO: reject the file
			}
			read_zstr(sf2.INFO.isng, szSoundEngine, 256);
			read_zstr(sf2.INFO.INAM, szName, 256);
			read_zstr(sf2.INFO.irom, szROM, 256);
			read_versiontag(sf2.INFO.iver, iver, 256);
			read_zstr(sf2.INFO.ICRD, szDate, 256);
			read_zstr(sf2.INFO.IENG, szCreator, 256);
			read_zstr(sf2.INFO.IPRD, szProduct, 256);
			read_zstr(sf2.INFO.ICOP, szCopyright, 256);
			read_zstr(sf2.INFO.ICMT, szComment, 65536);
			read_zstr(sf2.INFO.ISFT, szTools, 256);

			SF2_DEBUG_OUTPUT("Storing sample data offsets...\n");
			//Get sample data offsets of smpl and sm24 subchunks of sdta-list chunk
			sample_data_offset = sf2.sdta.smpl->data_offset;
			if(sf2.sdta.sm24)
				sample_data_24_offset = sf2.sdta.sm24->data_offset;
			else
				sample_data_24_offset = 0;

			//The HYDRA Data Structure 
			//========================================================================
			SF2_DEBUG_OUTPUT("Reading HYDRA data...\n");
			//The PHDR sub-chunk is a required sub-chunk listing all
			//presets within the SoundFont compatible file
			s->setpos(sf2.pdta.phdr->data_offset);
			hydra.phdr.resize(sf2.pdta.phdr->size/38);
			for(auto& preset : hydra.phdr)
			{
				preset = new HYDRA::sfPresetHeader;
				read_field(preset->achPresetName);
				//null-terminate preset name
				//but why do I have to do this anyway? Standard says I should reject it!
				preset->achPresetName[19] = 0;
				read_field(preset->wPreset);
				read_field(preset->wBank);
				read_field(preset->wPresetBagNdx);
				read_field(preset->dwLibrary);
				read_field(preset->dwGenre);
				read_field(preset->dwMorphology);
			}
			//The PBAG sub-chunk is a required sub-chunk listing all
			//preset zones within the SoundFont compatible file.
			//
			/*If a preset has more than one zone, the first zone may be a global zone.
			A global zone is determined by the fact that the last
			generator in the list is not an Instrument generator.
			All generator lists must contain at least one generator with one
			exception - if a global zone exists for which there are
			no generators but only modulators. The modulator lists can contain
			zero or more modulators. */
			s->setpos(sf2.pdta.pbag->data_offset);
			hydra.pbag.resize(sf2.pdta.pbag->size/4);
			for(auto& preset_zone : hydra.pbag)
			{
				preset_zone = new HYDRA::sfPresetBag;
				read_field(preset_zone->wGenNdx);
				read_field(preset_zone->wModNdx);
			}
			//The PMOD sub-chunk is a required sub-chunk listing all
			//preset zone modulators within the SoundFont compatible file.
			//
			/*Modulators in the PMOD sub-chunk act as additively
			relative modulators with respect to those in the IMOD sub-chunk.
			In other words, a PMOD modulator can increase or
			decrease the amount of an IMOD modulator. */
			s->setpos(sf2.pdta.pmod->data_offset);
			hydra.pmod.resize(sf2.pdta.pmod->size/10);
			for(auto& modulator : hydra.pmod)
			{
				modulator = new HYDRA::sfModList;
				read_field(modulator->sfModSrcOper);
				read_field(modulator->sfModDestOper);
				read_field(modulator->modAmount);
				read_field(modulator->sfModAmtSrcOper);
				read_field(modulator->sfModTransOper);
			}
			//The PGEN chunk is a required chunk containing a list
			//of preset zone generators for each preset zone within the SoundFont
			//compatible file.
			//
			/*The preset zones wGenNdx points to the first generator
			for that preset zone. Unless the zone is a global zone, the last
			generator in the list is an Instrument generator,
			whose value is a pointer to the instrument associated with that zone.
			If a key range generator exists for the preset zone,
			it is always the first generator in the list for that preset zone.
			If a velocity range generator exists for the preset zone,
			it will only be preceded by a key range generator. If any generators follow an
			Instrument generator, they will be ignored.

			A generator is defined by its sfGenOper.
			All generators within a zone must have a unique sfGenOper enumerator.
			If a second generator is encountered with the same sfGenOper enumerator
			as a previous generator with the same zone, the first
			generator will be ignored.

			Generators in the PGEN sub-chunk are applied relative to generators
			in the IGEN sub-chunk in an additive manner.
			In other words, PGEN generators increase or decrease the value
			of an IGEN generator. */
			s->setpos(sf2.pdta.pgen->data_offset);
			hydra.pgen.resize(sf2.pdta.pgen->size/4);
			for(auto& generator : hydra.pgen)
			{
				generator = new HYDRA::sfGenList;
				read_field(generator->sfGenOper);
				read_field(generator->genAmount);
			}
			//The inst sub-chunk is a required sub-chunk listing all
			//instruments within the SoundFont compatible file.
			s->setpos(sf2.pdta.inst->data_offset);
			hydra.inst.resize(sf2.pdta.inst->size/22);
			for(auto& instrument : hydra.inst)
			{
				instrument = new HYDRA::sfInst;
				read_field(instrument->achInstName);
				//null-terminate instrument name
				//you think it's funny not to terminate a string, soundfont editing software?
				instrument->achInstName[19] = 0;
				read_field(instrument->wInstBagNdx);
			}
			//The IBAG sub-chunk is a required sub-chunk listing all
			//instrument zones within the SoundFont compatible file. 
			//
			/*The first zone in a given instrument is located at that
			instruments wInstBagNdx. The number of zones in the instrument is
			determined by the difference between the next instruments wInstBagNdx
			and the current wInstBagNdx.

			If an instrument has more than one zone, the first zone may be a global zone.
			A global zone is determined by the fact that the last generator in the list
			is not a sampleID generator. All generator lists must contain at least
			one generator with one exception - if a global zone exists for which there
			are no generators but only modulators. The modulator lists can contain
			zero or more modulators. */
			s->setpos(sf2.pdta.ibag->data_offset);
			hydra.ibag.resize(sf2.pdta.ibag->size/4);
			for(auto& instrument_zone : hydra.ibag)
			{
				instrument_zone = new HYDRA::sfInstBag;
				read_field(instrument_zone->wInstGenNdx);
				read_field(instrument_zone->wInstModNdx);
			}
			//The IMOD sub-chunk is a required sub-chunk listing all
			//instrument zone modulators within the SoundFont compatible file.
			//
			/*The zones wInstModNdx points to the first modulator for that zone,
			and the number of modulators present for a zone is determined
			by the difference between the next higher zones wInstModNdx
			and the current zones wModNdx.
			A difference of zero indicates there are no modulators in this zone.

			Modulators in the IMOD sub-chunk are absolute.
			This means that an IMOD modulator replaces, rather than adds to, a
			default modulator. However the effect of a modulator on a generator
			is additive, IE the output of a modulator adds to a generator value. */
			s->setpos(sf2.pdta.imod->data_offset);
			hydra.imod.resize(sf2.pdta.imod->size/10);
			for(auto& instrument_zone_modulator : hydra.imod)
			{
				instrument_zone_modulator = new HYDRA::sfInstModList;
				read_field(instrument_zone_modulator->sfModSrcOper);
				read_field(instrument_zone_modulator->sfModDestOper);
				read_field(instrument_zone_modulator->modAmount);
				read_field(instrument_zone_modulator->sfModAmtSrcOper);
				read_field(instrument_zone_modulator->sfModTransOper);
			}
			//The IGEN chunk is a required chunk containing a list of zone generators
			//for each instrument zone within the SoundFont compatible file.
			//
			/*The zones wInstGenNdx points to the first generator for that zone.
			Unless the zone is a global zone, the last generator in the list is
			a sampleID generator, whose value is a pointer to the sample associated
			with that zone. If a key range generator exists for the zone, it is always
			the first generator in the list for that zone. If a velocity range generator
			exists for the zone, it will only be preceded by a key range generator.
			If any generators follow a sampleID generator, they will be ignored.

			Generators in the IGEN sub-chunk are absolute in nature.
			This means that an IGEN generator replaces, rather than adds to,
			the default value for the generator. */
			s->setpos(sf2.pdta.igen->data_offset);
			hydra.igen.resize(sf2.pdta.igen->size/4);
			for(auto& zone_generator : hydra.igen)
			{
				zone_generator = new HYDRA::sfInstGenList;
				read_field(zone_generator->sfGenOper);
				read_field(zone_generator->genAmount);
			}
			//The SHDR chunk is a required sub-chunk listing all samples within the smpl
			//sub-chunk and any referenced ROM samples.
			//
			/*The values of dwStart, dwEnd, dwStartloop, and dwEndloop must all be within
			the range of the sample data field included in the SoundFont compatible bank
			or referenced in the sound ROM. Also, to allow a variety of hardware platforms
			to be able to reproduce the data, the samples have a minimum length of 48 data
			points, a minimum loop size of 32 data points and a minimum of 8 valid points
			prior to dwStartloop and after dwEndloop.
			Thus dwStart must be less than dwStartloop-7, dwStartloop must be less than
			dwEndloop-31, and dwEndloop must be less than dwEnd-7.
			If these constraints are not met, the sound may optionally not be played
			if the hardware cannot support artifact-free playback for the parameters given.

			Sample rate values of greater than 50000 or less than 400 may not be reproducible
			by some hardware platforms and should be avoided. A value of zero is illegal.
			If an illegal or impractical value is encountered, the nearest practical value
			should be used.

			If sfSampleType indicates a mono sample, then wSampleLink is undefined and
			its value should be conventionally zero, but will be ignored regardless of value.
			If sfSampleType indicates a left or right sample, then wSampleLink is the sample
			header index of the associated right or left stereo sample respectively.
			Both samples should be played entirely syncrhonously, with their pitch controlled
			by the right samples generators. All non-pitch generators should apply as normal;
			in particular the panning of the individual samples to left and right should
			be accomplished via the pan generator.
			Left-right pairs should always be found within the same instrument.
			Note also that no instrument should be designed in which it is possible to
			activate more than one instance of a particular stereo pair. The linked sample
			type is not currently fully defined in the SoundFont 2 specification,
			but will ultimately support a circularly linked list of samples using 
			wSampleLink. Note that this enumeration is two bytes in length. */
			s->setpos(sf2.pdta.shdr->data_offset);
			hydra.shdr.resize(sf2.pdta.shdr->size/46);
			for(auto& sample : hydra.shdr)
			{
				sample = new HYDRA::sfSample;
				read_field(sample->achSampleName);
				//null-terminate sample name
				//unfortunately, some soundfonts don't do that!
				sample->achSampleName[19] = 0;
				read_field(sample->dwStart);
				read_field(sample->dwEnd);
				read_field(sample->dwStartloop);
				read_field(sample->dwEndloop);
				read_field(sample->dwSampleRate);
				read_field(sample->byOriginalKey);
				read_field(sample->chCorrection);
				read_field(sample->wSampleLink);
				s->read(&sample->sfSampleType, sizeof(WORD));
			}

#undef read_zstr
#undef read_versiontag
#undef read_field

			//save stream
			stream = s;

			//Translate HYDRA structure data
			//========================================================================
			SF2_DEBUG_OUTPUT("Parsing HYDRA data...\n");
			SF2_DEBUG_OUTPUT("Scanning banks...\n");
			//Scan banks
			for(size_t i = 0; i < hydra.phdr.size(); ++i)
			{
				//check for already listed ones
				for(auto b : banks)
				{
					if(b->num == hydra.phdr[i]->wBank)
					{
						//already exists, skip
						goto next_preset;
					}
				}
				//not listed yet, add to the list
				banks.push_back(new Bank);
				banks.back()->num = hydra.phdr[i]->wBank;
			next_preset:{}
			}

			//Load samples
			SF2_DEBUG_OUTPUT("Loading samples...\n");
			samples.resize(hydra.shdr.size()-1);
			for(auto& sample : samples) sample = new Sample;

			{
				size_t i = 0;
				for(auto sample = hydra.shdr[i]; sample != hydra.shdr.back(); ++i, sample = hydra.shdr[i])
				{
					samples[i]->name = (const char*)sample->achSampleName;
					samples[i]->sample_rate = sample->dwSampleRate;
					//need to subtract stream offset from loop points
					//to get them to be local to the data buffer pointer
					samples[i]->loop_start = sample->dwStartloop - sample->dwStart;
					samples[i]->loop_end = sample->dwEndloop - sample->dwStart;
					samples[i]->original_key = sample->byOriginalKey;
					samples[i]->correction = sample->chCorrection;
					samples[i]->sample_type = sample->sfSampleType;
					//check sfSampleType and try to fix
					if(!CheckSampleLinkType(samples[i]->sample_type))
					{
						samples[i]->sample_type = SFSampleLink::monoSample;
					}
					if(samples[i]->sample_type != SFSampleLink::monoSample)
						samples[i]->linked_sample = samples[sample->wSampleLink];
					else samples[i]->linked_sample = nullptr;
					//Don't load sample data but prepare it for on demand streaming
					//or manually requested loading
					samples[i]->data_stream_offset = sample->dwStart;
					samples[i]->size = sample->dwEnd/* + 46*/ - sample->dwStart;
					samples[i]->data = nullptr;

					//test load
					//samples[i]->load_data(*this, s);
				}
			}

			//Load instruments
			SF2_DEBUG_OUTPUT("Loading instruments...\n");
			instruments.resize(hydra.inst.size()-1);
			{
				size_t i = 0;
				for(auto inst = hydra.inst[i]; inst != hydra.inst.back(); ++i, inst = hydra.inst[i])
				{

					instruments[i] = new Instrument;
					instruments[i]->name = (const char*)hydra.inst[i]->achInstName;
					{
						Instrument::Zone* global_zone = nullptr;

						//test
						/*if(instruments[i]->name[0] == 'S' &&
						   instruments[i]->name[1] == 'y' &&
						   instruments[i]->name[2] == 'n' &&
						   instruments[i]->name[3] == 't' &&
						   instruments[i]->name[4] == 'h' &&
						   instruments[i]->name[5] == ' ' &&
						   instruments[i]->name[6] == 'S' &&
						   instruments[i]->name[7] == 't' &&
						   instruments[i]->name[8] == 'r' &&
						   instruments[i]->name[9] == 'i' &&
						   instruments[i]->name[10] == 'n' &&
						   instruments[i]->name[11] == 'g' &&
						   instruments[i]->name[12] == 's' &&
						   instruments[i]->name[13] == ' ' &&
						   instruments[i]->name[14] == '2')
						{
							int breakpoint_here = 1;
						}*/

						size_t j = inst->wInstBagNdx;
						//first zone of the next instrument marks the end of the current
						//instrument zone list
						auto iz_end = hydra.ibag[hydra.inst[i+1]->wInstBagNdx];
						//for each instrument zone
						for(auto iz = hydra.ibag[j]; iz != iz_end; ++j, iz = hydra.ibag[j])
						{							
							auto split = new Instrument::Zone;
							//Only instrument generators have default values
							split->modEnv.SetToDefault();
							split->volEnv.SetToDefault();

							/*Points below (until noted) apply to Value Generators ONLY. */
							/*
							A generator in a global instrument zone that is identical to a default
							generator supersedes or replaces the default generator. 
							*/
							if(global_zone)
							{
								//split->CopyValueGenerators(*global_zone);
								*split = *global_zone;
							}

							size_t k = iz->wInstGenNdx;
							auto igen_end = hydra.igen[hydra.ibag[j+1]->wInstGenNdx];
							//for each generator
							for(auto ig = hydra.igen[k]; ig != igen_end; ++k, ig = hydra.igen[k])
							{
								/*
								A generator in a local instrument zone that is identical to a default
								generator or to a generator in a global instrument zone supersedes or
								replaces that generator. 
								*/
								switch(ig->sfGenOper.enumeration)
								{
								case SFGenerator::GenType::sampleID:
								{
									split->sample =	samples[ig->genAmount.wAmount];
									break;
								}
								case SFGenerator::GenType::startAddrsOffset:
								{
									split->start_offset += ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::startAddrsCoarseOffset:
								{
									split->start_offset += ig->genAmount.shAmount*32768;
									break;
								}
								case SFGenerator::GenType::endAddrsOffset:
								{
									split->end_offset += ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::endAddrsCoarseOffset:
								{
									split->end_offset += ig->genAmount.shAmount*32768;
									break;
								}
								case SFGenerator::GenType::startloopAddrsOffset:
								{
									split->loop_start_offset += ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::startloopAddrsCoarseOffset:
								{
									split->loop_start_offset += ig->genAmount.shAmount*32768;
									break;
								}
								case SFGenerator::GenType::endloopAddrsOffset:
								{
									split->loop_end_offset += ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::endloopAddrsCoarseOffset:
								{
									split->loop_end_offset += ig->genAmount.shAmount*32768;
									break;
								}
								case SFGenerator::GenType::modLfoToPitch:
								{
									split->modLFO_to_pitch = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::vibLfoToPitch:
								{
									split->vibLFO_to_pitch = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::modEnvToPitch:
								{
									split->modEnv_to_pitch = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::initialFilterFc:
								{
									split->filter_freq = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::initialFilterQ:
								{
									split->filter_q = ig->genAmount.shAmount / 10.0f;
									break;
								}
								case SFGenerator::GenType::modLfoToFilterFc:
								{
									split->modLFO_to_filter_fc = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::modEnvToFilterFc:
								{
									split->modEnv_to_filter_fc = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::modLfoToVolume:
								{
									split->modLFO_to_volume = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::chorusEffectsSend:
								{
									split->chorus_send = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::reverbEffectsSend:
								{
									split->reverb_send = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::pan:
								{
									split->pan = float(ig->genAmount.shAmount)/1000.0f;
									break;
								}
								case SFGenerator::GenType::delayModLFO:
								{
									split->modLFO.delay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::freqModLFO:
								{
									split->modLFO.frequency = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::delayVibLFO:
								{
									split->vibLFO.delay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::freqVibLFO:
								{
									split->vibLFO.frequency = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::delayModEnv:
								{
									split->modEnv.delay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::attackModEnv:
								{
									split->modEnv.attack = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::holdModEnv:
								{
									split->modEnv.hold = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::decayModEnv:
								{
									split->modEnv.decay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::sustainModEnv:
								{
									split->modEnv.sustain = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::releaseModEnv:
								{
									split->modEnv.release = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::keynumToModEnvHold:
								{
									split->modEnv.keynumToHold = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::keynumToModEnvDecay:
								{
									split->modEnv.keynumToDecay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::delayVolEnv:
								{
									split->volEnv.delay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::attackVolEnv:
								{
									split->volEnv.attack = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::holdVolEnv:
								{
									split->volEnv.hold = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::decayVolEnv:
								{
									split->volEnv.decay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::sustainVolEnv:
								{
									split->volEnv.sustain = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::releaseVolEnv:
								{
									split->volEnv.release = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::keynumToVolEnvHold:
								{
									split->volEnv.keynumToHold = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::keynumToVolEnvDecay:
								{
									split->volEnv.keynumToDecay = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::keyRange:
								{
									split->key_low = ig->genAmount.ranges.byLo;
									split->key_high = ig->genAmount.ranges.byHi;
									break;
								}
								case SFGenerator::GenType::velRange:
								{
									split->vel_low = ig->genAmount.ranges.byLo;
									split->vel_high = ig->genAmount.ranges.byHi;
									break;
								}
								case SFGenerator::GenType::keynum:
								{
									split->keynum = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::velocity:
								{
									split->velocity = ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::initialAttenuation:
								{
									split->attenuation = float(ig->genAmount.shAmount)/10.0f;
									break;
								}
								case SFGenerator::GenType::coarseTune:
								{
									split->tune += ig->genAmount.shAmount*100;
									break;
								}
								case SFGenerator::GenType::fineTune:
								{
									split->tune += ig->genAmount.shAmount;
									break;
								}
								case SFGenerator::GenType::sampleModes:
								{
									switch(ig->genAmount.wAmount & 3)
									{
									case 0: split->loop_mode = LoopMode::None; break;
									case 1: split->loop_mode = LoopMode::Continuous; break;
									case 2: split->loop_mode = LoopMode::None; break;
									case 3: split->loop_mode = LoopMode::Sustain; break;
									}
									break;
								}
								case SFGenerator::GenType::scaleTuning:
								{
									//[0;1] range
									split->scale_tuning = float(ig->genAmount.shAmount)/100.0f;
									break;
								}
								case SFGenerator::GenType::exclusiveClass:
								{
									split->exclusive_class = ig->genAmount.wAmount;
									break;
								}
								case SFGenerator::GenType::overridingRootKey:
								{
									split->root_key = ig->genAmount.shAmount;
									break;
								}
								}
							}
							//if last generator is not a sampleID generator
							if(split->sample == nullptr)
							{
								//also must be more than one zone for a global one to exist
								//and it also must be first zone in the list
								if(j == inst->wInstBagNdx && (hydra.inst[i+1]->wInstBagNdx - inst->wInstBagNdx) > 1)
								{
									//global zone detected
									//instruments[i]->global_zone = split;
									global_zone = split;
								} else goto discard_zone;
							} else instruments[i]->splits.push_back(split);

							continue;
						discard_zone:
							{
								delete instruments[i]->splits.back();
								instruments[i]->splits.erase(
									instruments[i]->splits.begin()+instruments[i]->splits.size()-1
								);
							}
						}
						delete global_zone;
					}

					//instrument zone list
					auto zones = hydra.ibag[hydra.inst[i]->wInstBagNdx];
					auto zone_gen = hydra.igen[zones->wInstGenNdx];
					int dummy_dum = 0;
				}
			}

			//Load presets
			SF2_DEBUG_OUTPUT("Loading presets...\n");
			for(size_t i = 0; i < hydra.phdr.size()-1; ++i)
			{
				auto p = new Preset;
				p->name = (const char*)hydra.phdr[i]->achPresetName;
				p->num = hydra.phdr[i]->wPreset;
				//find bank
				for(auto& bank : banks)
				{
					if(bank->num == hydra.phdr[i]->wBank)
					{
						//add preset to the bank
						bank->presets.push_back(p);
						break;
					}
				}

				//a global zone is a first zone and may only exist if
				//there is more than one zone for a given preset
				//Preset::Zone* global_zone = nullptr;
				auto global_zone_begin = hydra.pgen.begin();
				auto global_zone_end = global_zone_begin;

				if((hydra.phdr[i+1]->wPresetBagNdx - hydra.phdr[i]->wPresetBagNdx) > 1)
				{
					//Get first zone of the preset
					auto zone = hydra.pbag[hydra.phdr[i]->wPresetBagNdx];
					//Get first generator of the next zone to acts as an end of the current list.
					//For this to work, there exists a dummy terminator zone in the end of the
					//PGEN chunk just so we can iterate all of them using this method.
					auto pgen_end = hydra.pgen.begin()+hydra.pbag[hydra.phdr[i]->wPresetBagNdx+1]->wGenNdx;
					//if zone isn't empty
					if(pgen_end != hydra.pgen.begin())
					{
						//if last generator isn't an instrument generator
						if((*(pgen_end - 1))->sfGenOper != SFGenerator::GenType::instrument)
						{
							//store global zone iterators
							global_zone_begin = hydra.pgen.begin()+zone->wGenNdx;
							global_zone_end = pgen_end;
						}
					}
				}

				{
					size_t j = hydra.phdr[i]->wPresetBagNdx;
					//first zone of the next preset marks the end of the current
					//preset zone list
					auto pz_end = hydra.pbag[hydra.phdr[i+1]->wPresetBagNdx];
					//for each preset zone
					for(auto pz = hydra.pbag[j]; pz != pz_end; ++j, pz = hydra.pbag[j])
					{
						//check if zone isn't empty
						if(hydra.pgen.begin()+pz->wGenNdx == hydra.pgen.begin()+hydra.pbag[j+1]->wGenNdx)
						{
							//discard zone
							continue;
						}
						//check if last generator isn't an instrument generator
						if((*(hydra.pgen.begin()+hydra.pbag[j+1]->wGenNdx - 1))->sfGenOper != SFGenerator::GenType::instrument)
						{
							//discard zone
							continue;
						}

						/*Points below (until noted) apply to Value Generators ONLY. */
						/*
						A generator in a local preset zone that is identical to a generator in
						a global preset zone supersedes or replaces that generator in the global
						preset zone. That generator then has its effects added to the destination-summing
						node of all zones in the given instrument.
						*/

						std::vector<HYDRA::sfGenList*> generators;
						//copy zone generators and supersede identical globals with locals
						{							
							//collect all generators from the global zone
							for(auto pg = global_zone_begin; pg != global_zone_end; ++pg)
							{
								generators.push_back(*pg);
							}

							//for each generator of the current preset zone
							for(auto pg = hydra.pgen.begin()+pz->wGenNdx; pg != hydra.pgen.begin()+hydra.pbag[j+1]->wGenNdx; ++pg)
							{
								//try to find identical generator
								for(auto gen = generators.begin(); gen != generators.end(); ++gen)
								{
									if((*gen)->sfGenOper == (*pg)->sfGenOper)
									{
										//replace global generator with local
										*gen = *pg;
										goto pg_next_generator;
									}
								}
								//generator is unique, has its effect added
								generators.push_back(*pg);
							pg_next_generator:{}
							}
						}
						//Get the zone instrument
						//Instrument* instrument = instruments[(*(hydra.pgen.begin()+hydra.pbag[j+1]->wGenNdx - 1))->genAmount.wAmount];
						//Initialize preset zone with instrument's zone
						//*layer = instrument->splits;
						//TODO: possibly precompute (combine with preset zones) splits for each preset

						//check if no generators exist for this zone
						if(generators.empty()) continue;

						Preset::Zone* layer = new Preset::Zone;

						//for each generator
						for(auto pg = generators.begin(); pg != generators.end(); ++pg)
						{
							switch((*pg)->sfGenOper.enumeration)
							{
							case SFGenerator::GenType::instrument:
							{
								layer->instrument = instruments[(*pg)->genAmount.wAmount];
								break;
							}
							case SFGenerator::GenType::modLfoToPitch:
							{
								layer->modLFO_to_pitch = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::vibLfoToPitch:
							{
								layer->vibLFO_to_pitch = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::modEnvToPitch:
							{
								layer->modEnv_to_pitch = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::initialFilterFc:
							{
								layer->filter_freq = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::initialFilterQ:
							{
								layer->filter_q = (*pg)->genAmount.shAmount / 10.0f;
								break;
							}
							case SFGenerator::GenType::modLfoToFilterFc:
							{
								layer->modLFO_to_filter_fc = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::modEnvToFilterFc:
							{
								layer->modEnv_to_filter_fc = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::modLfoToVolume:
							{
								layer->modLFO_to_volume = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::chorusEffectsSend:
							{
								layer->chorus_send = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::reverbEffectsSend:
							{
								layer->reverb_send = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::pan:
							{
								layer->pan = float((*pg)->genAmount.shAmount)/1000.0f;
								break;
							}
							case SFGenerator::GenType::delayModLFO:
							{
								layer->modLFO.delay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::freqModLFO:
							{
								layer->modLFO.frequency = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::delayVibLFO:
							{
								layer->vibLFO.delay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::freqVibLFO:
							{
								layer->vibLFO.frequency = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::delayModEnv:
							{
								layer->modEnv.delay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::attackModEnv:
							{
								layer->modEnv.attack = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::holdModEnv:
							{
								layer->modEnv.hold = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::decayModEnv:
							{
								layer->modEnv.decay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::sustainModEnv:
							{
								layer->modEnv.sustain = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::releaseModEnv:
							{
								layer->modEnv.release = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::keynumToModEnvHold:
							{
								layer->modEnv.keynumToHold = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::keynumToModEnvDecay:
							{
								layer->modEnv.keynumToDecay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::delayVolEnv:
							{
								layer->volEnv.delay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::attackVolEnv:
							{
								layer->volEnv.attack = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::holdVolEnv:
							{
								layer->volEnv.hold = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::decayVolEnv:
							{
								layer->volEnv.decay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::sustainVolEnv:
							{
								layer->volEnv.sustain = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::releaseVolEnv:
							{
								layer->volEnv.release = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::keynumToVolEnvHold:
							{
								layer->volEnv.keynumToHold = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::keynumToVolEnvDecay:
							{
								layer->volEnv.keynumToDecay = (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::keyRange:
							{
								layer->key_low = (*pg)->genAmount.ranges.byLo;
								layer->key_high = (*pg)->genAmount.ranges.byHi;
								break;
							}
							case SFGenerator::GenType::velRange:
							{
								layer->vel_low = (*pg)->genAmount.ranges.byLo;
								layer->vel_high = (*pg)->genAmount.ranges.byHi;
								break;
							}
							case SFGenerator::GenType::initialAttenuation:
							{
								layer->attenuation = float((*pg)->genAmount.shAmount)/10.0f;
								break;
							}
							case SFGenerator::GenType::coarseTune:
							{
								layer->tune += (*pg)->genAmount.shAmount*100;
								break;
							}
							case SFGenerator::GenType::fineTune:
							{
								layer->tune += (*pg)->genAmount.shAmount;
								break;
							}
							case SFGenerator::GenType::scaleTuning:
							{
								//[0;1] range
								layer->scale_tuning = float((*pg)->genAmount.shAmount)/100.0f;
								break;
							}
							}
						}
						//add layer to the list
						p->layers.push_back(layer);
					}
				}
				//delete global_zone;
			}


			SF2_DEBUG_OUTPUT("Sorting banks...\n");
			//Sort banks
			std::sort(
				banks.begin(),
				banks.end(),
				[](Bank* a, Bank* b)
				{
					return a->num < b->num;
				}
			);
			SF2_DEBUG_OUTPUT("Sorting presets...\n");
			//Sort presets
			for(auto& bank : banks)
			{
				std::sort(
					bank->presets.begin(),
					bank->presets.end(),
					[](Preset* a, Preset* b)
					{
						return a->num < b->num;
					}
				);
			}

		}

		~SoundFont2()
		{
			for(auto i : banks) delete i;
			for(auto i : instruments) delete i;
			for(auto i : samples) delete i;
		}
	};
}
