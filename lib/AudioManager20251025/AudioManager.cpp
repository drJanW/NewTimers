#include "Globals.h"
#include "AudioManager.h"
#include "PlayFragment.h"
#include "PlaySentence.h"
#include "SonarPingGenerator.h"
#include "SDVoting.h"
#include "TimerManager.h"
#include <math.h>

namespace {
AudioOutputI2S_Metered* gMeterInstance = nullptr;
AudioManager* gAudioManagerInstance = nullptr;
constexpr uint32_t kAudioMeterIntervalMs = 50;

constexpr float SONAR_NEAR_MM = 120.0f;
constexpr float SONAR_FAR_MM = 1100.0f;
constexpr float SONAR_SILENCE_MM = 1350.0f;
constexpr float SONAR_TRIGGER_DELTA_MM = 90.0f;
constexpr uint32_t SONAR_MIN_INTERVAL_MS = 420;
constexpr uint32_t SONAR_MAX_INTERVAL_MS = 1800;
constexpr uint32_t SONAR_INTERVAL_EPSILON_MS = 30;
constexpr uint32_t SONAR_TRIGGER_DELAY_MS = 35;
constexpr float SONAR_BASE_DURATION_MS = 340.0f;
constexpr float SONAR_NOISE_NEAR = 0.24f;
constexpr float SONAR_NOISE_FAR = 0.08f;
} // namespace

void audioMeterTick();
void sonarPingTick();

AudioManager& AudioManager::instance() {
	static AudioManager inst;
	return inst;
}

AudioManager::AudioManager()
	: audioOutput()
{
	gAudioManagerInstance = this;
}

bool AudioOutputI2S_Metered::begin()
{
	_acc = 0;
	_cnt = 0;
	_publishDue = false;
	setAudioLevelRaw(0);

	gMeterInstance = this;

	auto &tm = TimerManager::instance();
	tm.cancel(audioMeterTick);
	if (!tm.create(kAudioMeterIntervalMs, 0, audioMeterTick)) {
		PF("[AudioMeter] Failed to start meter timer\n");
	}

	return AudioOutputI2S::begin();
}

bool AudioOutputI2S_Metered::ConsumeSample(int16_t sample[2])
{
	int32_t v = sample[0];
	_acc += static_cast<int64_t>(v) * static_cast<int64_t>(v);
	_cnt++;
	_publishDue = true;
	return AudioOutputI2S::ConsumeSample(sample);
}

void AudioOutputI2S_Metered::publishLevel()
{
	if (!_publishDue || _cnt == 0) {
		return;
	}

	_publishDue = false;
	float mean = static_cast<float>(_acc) / static_cast<float>(_cnt);
	uint16_t rms = static_cast<uint16_t>(sqrtf(mean));
	setAudioLevelRaw(rms);
	_acc = 0;
	_cnt = 0;
}

void audioMeterTick()
{
	if (gMeterInstance) {
		gMeterInstance->publishLevel();
	}
}

void AudioManager::releaseDecoder()
{
	if (audioMp3Decoder) {
		audioMp3Decoder->stop();
		delete audioMp3Decoder;
		audioMp3Decoder = nullptr;
	}
}

void AudioManager::releaseSource()
{
	if (audioFile) {
		delete audioFile;
		audioFile = nullptr;
	}
}

void AudioManager::finalizePlayback()
{
	releaseDecoder();
	releaseSource();

	setAudioLevelRaw(0);
	audioOutput.SetGain(getBaseGain() * getWebAudioLevel());
	setAudioBusy(false);
	setFragmentPlaying(false);
	setSentencePlaying(false);
}

void AudioManager::finalizePing()
{
	if (!audioPingGenerator) {
		return;
	}

	audioPingGenerator->stop();
	delete audioPingGenerator;
	audioPingGenerator = nullptr;

	if (!audioMp3Decoder) {
		setAudioBusy(false);
		setFragmentPlaying(false);
		setSentencePlaying(false);
		setAudioLevelRaw(0);
		audioOutput.SetGain(getBaseGain() * getWebAudioLevel());
	}
}

bool AudioManager::playSonarPing(const SonarPingProfile& profile)
{
	finalizePing();

	if (isFragmentPlaying()) {
		PlayAudioFragment::abortImmediate();
	}
	if (isSentencePlaying()) {
		PlaySentence::stop();
	}

	finalizePlayback();

	auto *ping = new SonarPingGenerator();
	if (!ping) {
		return false;
	}

	ping->configure(profile);

	const float baseGain = getBaseGain() * getWebAudioLevel();
	audioOutput.SetGain(baseGain);

	setAudioBusy(true);
	setFragmentPlaying(false);
	setSentencePlaying(false);
	setAudioLevelRaw(0);

	if (!ping->begin(nullptr, &audioOutput)) {
		delete ping;
		setAudioBusy(false);
		audioOutput.SetGain(baseGain);
		return false;
	}

	audioPingGenerator = ping;
	return true;
}

	void AudioManager::notifySonarDistance(float distanceMm)
	{
		SonarPingProfile profile;
		uint32_t intervalMs = 0;

		if (!buildSonarProfile(distanceMm, profile, intervalMs)) {
			cancelSonarTimer();
			return;
		}

		bool reschedule = !sonarState_.enabled;
		if (sonarState_.enabled) {
			uint32_t diff = (sonarState_.intervalMs > intervalMs)
													? (sonarState_.intervalMs - intervalMs)
													: (intervalMs - sonarState_.intervalMs);
			if (diff >= SONAR_INTERVAL_EPSILON_MS) {
				reschedule = true;
			}
		}

		bool immediate = false;
		if (sonarState_.hasDistance) {
			float delta = fabsf(distanceMm - sonarState_.lastDistance);
			if (delta >= SONAR_TRIGGER_DELTA_MM) {
				immediate = true;
			}
		}

		sonarState_.profile = profile;
		sonarState_.intervalMs = intervalMs;
		sonarState_.lastDistance = distanceMm;
		sonarState_.hasDistance = true;
		sonarState_.enabled = true;

		if (reschedule || immediate) {
			uint32_t delayMs = immediate ? SONAR_TRIGGER_DELAY_MS : intervalMs;
			scheduleSonar(delayMs);
		}
	}

void AudioManager::begin()
{
	audioOutput.SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
	audioOutput.begin();
	audioOutput.SetGain(getBaseGain() * getWebAudioLevel());
}

void AudioManager::stop()
{
	cancelSonarTimer();
	finalizePing();
	finalizePlayback();
}

void AudioManager::update()
{
	if (audioPingGenerator) {
		bool keepPing = audioPingGenerator->loop();
		if (!keepPing) {
			finalizePing();
		}
	}

	if (isSentencePlaying()) {
		PlaySentence::update();
	}

	if (audioMp3Decoder) {
		bool keep = audioMp3Decoder->loop();
		if (!keep) {
			finalizePlayback();
		}
	}
}

bool AudioManager::isBusy() const {
	return isAudioBusy();
}

bool AudioManager::isFragmentPlaying() const {
	return ::isFragmentPlaying();
}

bool AudioManager::isSentencePlaying() const {
	return ::isSentencePlaying();
}

bool AudioManager::startFragment(const AudioFragment& frag) {
	if (!PlayAudioFragment::start(frag)) {
		PF("[Audio] startFragment failed for %03u/%03u\n", frag.dirIndex, frag.fileIndex);
		return false;
	}
	return true;
}

void AudioManager::startTTS(const String& phrase) {
	PlaySentence::startTTS(phrase);
}

void AudioManager::stopFragment() {
	PlayAudioFragment::stop();
}

void AudioManager::setWebLevel(float value) {
	if (value < 0.0f) value = 0.0f;
	if (value > 1.0f) value = 1.0f;
	setWebAudioLevel(value);
	updateGain();
}

float AudioManager::getWebLevel() const {
	return getWebAudioLevel();
}

void AudioManager::capVolume(float maxValue) {
	if (maxValue < 0.0f) maxValue = 0.0f;
	if (maxValue > 1.0f) maxValue = 1.0f;
	float current = getWebAudioLevel();
	if (current > maxValue) {
		setWebAudioLevel(maxValue);
		updateGain();
	}
}

void AudioManager::updateGain() {
	PlayAudioFragment::updateGain();
	if (!isFragmentPlaying() && !isSentencePlaying() && !audioPingGenerator) {
		audioOutput.SetGain(getBaseGain() * getWebAudioLevel());
	}
}

void AudioManager::cancelSonarTimer()
{
	TimerManager::instance().cancel(sonarPingTick);
	sonarState_ = SonarState{};
}

void AudioManager::scheduleSonar(uint32_t delayMs)
{
	auto &tm = TimerManager::instance();
	tm.cancel(sonarPingTick);
	if (!sonarState_.enabled) {
		return;
	}

	if (delayMs == 0U) {
		delayMs = SONAR_MIN_INTERVAL_MS;
	}

	if (!tm.create(delayMs, 1, sonarPingTick)) {
		PF("[Audio] Failed to schedule sonar ping (%lu ms)\n", static_cast<unsigned long>(delayMs));
		sonarState_.enabled = false;
	} else {
		PF("[Audio] Sonar scheduled in %lu ms (interval %lu)\n",
			 static_cast<unsigned long>(delayMs),
			 static_cast<unsigned long>(sonarState_.intervalMs));
	}
}

void AudioManager::handleSonarTick()
{
	if (!sonarState_.enabled) {
		return;
	}

	if (audioPingGenerator && audioPingGenerator->isRunning()) {
		uint32_t waitMs = audioPingGenerator->remainingMs();
		if (waitMs < SONAR_TRIGGER_DELAY_MS) {
			waitMs = SONAR_TRIGGER_DELAY_MS;
		}
		PF("[Audio] Sonar busy, retry in %lu ms\n", static_cast<unsigned long>(waitMs));
		scheduleSonar(waitMs);
		return;
	}

	PF("[Audio] Sonar tick: start %.1fHz → %.1fHz amp %.2f noise %.2f\n",
		 static_cast<double>(sonarState_.profile.startHz),
		 static_cast<double>(sonarState_.profile.endHz),
		 static_cast<double>(sonarState_.profile.amplitude),
		 static_cast<double>(sonarState_.profile.noise));
	playSonarPing(sonarState_.profile);
	scheduleSonar(sonarState_.intervalMs);
}

bool AudioManager::buildSonarProfile(float distanceMm, SonarPingProfile& profile, uint32_t& intervalMs)
{
	if (distanceMm <= 0.0f) {
		return false;
	}
	if (distanceMm >= SONAR_SILENCE_MM) {
		return false;
	}

	const float span = SONAR_FAR_MM - SONAR_NEAR_MM;
	float clamped = distanceMm;
	if (clamped < SONAR_NEAR_MM) clamped = SONAR_NEAR_MM;
	if (clamped > SONAR_FAR_MM) clamped = SONAR_FAR_MM;
	const float norm = span > 1.0f ? (clamped - SONAR_NEAR_MM) / span : 0.0f;
	const float closeness = 1.0f - norm;

	profile.durationMs = SONAR_BASE_DURATION_MS;
	profile.startHz = 480.0f - norm * 80.0f;
	profile.endHz = 980.0f + closeness * 380.0f;
	profile.amplitude = 0.18f + closeness * 0.62f;
	profile.noise = SONAR_NOISE_FAR + closeness * (SONAR_NOISE_NEAR - SONAR_NOISE_FAR);

	float intervalF = static_cast<float>(SONAR_MIN_INTERVAL_MS) +
										 norm * static_cast<float>(SONAR_MAX_INTERVAL_MS - SONAR_MIN_INTERVAL_MS);
	intervalMs = static_cast<uint32_t>(intervalF);
	if (intervalMs < SONAR_MIN_INTERVAL_MS) intervalMs = SONAR_MIN_INTERVAL_MS;
	if (intervalMs > SONAR_MAX_INTERVAL_MS) intervalMs = SONAR_MAX_INTERVAL_MS;
	return true;
}

void sonarPingTick()
{
	if (gAudioManagerInstance) {
		gAudioManagerInstance->handleSonarTick();
	}
}
