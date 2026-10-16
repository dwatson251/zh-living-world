// Living World client audio: a replacement for the retail Miles Sound System manager (Miles is proprietary and not part of EA's released source).
// Sound effects, speech and music are read out of the game's archives through TheFileSystem and played with miniaudio (src/audio). The request,
// loop, limit and volume rules follow GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp. 3D sounds get distance volume and left/right pan.
// The dedicated server and `UM_NOSOUND=1` keep the silent NullAudioManager.
#include "PreRTS.h"
#include "null/NullAudioManager.h"
#include "audio/um_ma.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioRequest.h"
#include "Common/AudioSettings.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioAffect.h"
#include "Common/GameSounds.h"
#include "Common/GameMusic.h"
#include "Common/FileSystem.h"
#include "Common/file.h"
#include "Common/GameCommon.h"
#include <list>
#include <map>
#include <vector>
#include <stdio.h>
#include <string>
#include <math.h>
#include <ctype.h>

namespace
{
enum PlayKind { PK_Sample, PK_3D, PK_Stream };
struct UmPlaying
{
	AudioEventRTS *ev; UmaSound *snd; PlayKind kind; bool stopped, requestStop, cleanupEvent, paused; int loopsDone;
	UmPlaying() : ev(NULL), snd(NULL), kind(PK_Sample), stopped(false), requestStop(false), cleanupEvent(true), paused(false), loopsDone(0) {}
};
typedef std::list<UmPlaying *> PlayList;
}

class UmAudioManager : public NullAudioManager
{
public:
	UmAudioManager() : m_resumeMs(0), m_resumeUntil(0), m_resumeSwapped(false), m_cacheBytes(0), m_deviceOk(false), m_started(0), m_failed(0), m_lastLog(0), m_ambientPaused(false) {}
	virtual ~UmAudioManager() { stopAllNow(); uma_shutdown(); }

	virtual Bool isMusicAlreadyLoaded(void) const { return TRUE; }   // no "insert the CD" prompt

	virtual void init()
	{
		AudioManager::init();
		openDevice();
	}
	virtual void reset()
	{
		stopAllNow();
		removeAllAudioRequests();
		AudioManager::reset();
	}
	virtual void update()
	{
		umResumeTick();
		AudioManager::update();
		processRequestList();
		processPlayingList();
		logStats();
	}

	// ---- device
	virtual void openDevice() { if (!m_deviceOk) { m_deviceOk = uma_init() != 0; printf("[um] audio: %s (device: %s)\n", m_deviceOk ? "playback started" : "NO audio device, running silent", uma_device_name()); fflush(stdout); } }
	virtual void closeDevice() { stopAllNow(); uma_shutdown(); m_deviceOk = false; }
	virtual UnsignedInt getNum2DSamples() const { return m_audioSettings ? m_audioSettings->m_sampleCount2D : 32; }
	virtual UnsignedInt getNum3DSamples() const { return m_audioSettings ? m_audioSettings->m_sampleCount3D : 24; }
	virtual UnsignedInt getNumStreams() const { return m_audioSettings ? m_audioSettings->m_streamCount : 4; }

	// ---- stopping, pausing
	virtual void stopAudio(AudioAffect which)
	{
		forEach(which, &UmAudioManager::markStopped);
	}
	virtual void pauseAudio(AudioAffect which) { forEach(which, &UmAudioManager::doPause); }
	virtual void resumeAudio(AudioAffect which) { forEach(which, &UmAudioManager::doResume); }
	virtual void pauseAmbient(Bool shouldPause) { m_ambientPaused = shouldPause != 0; }
	virtual void killAudioEventImmediately(AudioHandle h)
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); ++it)
				if ((*it)->ev->getPlayingHandle() == h) { release(*it); lists[i]->erase(it); return; }
	}

	// ---- music
	virtual void nextMusicTrack() { changeTrack(true); }
	virtual void prevMusicTrack() { changeTrack(false); }
	virtual Bool isMusicPlaying() const { return findMusic() != NULL; }
	virtual Bool hasMusicTrackCompleted(const AsciiString &trackName, Int times) const
	{
		const UmPlaying *p = findMusic();
		return p && p->ev->getEventName() == trackName && p->loopsDone >= times;
	}
	virtual AsciiString getMusicTrackName() const
	{
		for (std::list<AudioRequest *>::const_iterator a = m_audioRequests.begin(); a != m_audioRequests.end(); ++a)
			if ((*a)->m_request == AR_Play && (*a)->m_usePendingEvent && (*a)->m_pendingEvent->getAudioEventInfo() && (*a)->m_pendingEvent->getAudioEventInfo()->m_soundType == AT_Music)
				return (*a)->m_pendingEvent->getEventName();
		const UmPlaying *p = findMusic();
		return p ? p->ev->getEventName() : AsciiString::TheEmptyString;
	}

	// ---- queries used by the shared audio code
	virtual Bool isCurrentlyPlaying(AudioHandle h)
	{
		const PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::const_iterator it = lists[i]->begin(); it != lists[i]->end(); ++it)
				if ((*it)->ev->getPlayingHandle() == h) return TRUE;
		for (std::list<AudioRequest *>::iterator a = m_audioRequests.begin(); a != m_audioRequests.end(); ++a)
			if ((*a)->m_usePendingEvent && (*a)->m_pendingEvent->getPlayingHandle() == h) return TRUE;
		return FALSE;
	}
	virtual Bool doesViolateLimit(AudioEventRTS *event) const
	{
		Int limit = event->getAudioEventInfo()->m_limit;
		if (limit == 0) return FALSE;
		Int total = 0, requested = 0;
		const PlayList &l = event->isPositionalAudio() ? m_sounds3D : m_sounds;
		for (PlayList::const_iterator it = l.begin(); it != l.end(); ++it)
			if ((*it)->ev->getEventName() == event->getEventName()) { if (total == 0) event->setHandleToKill((*it)->ev->getPlayingHandle()); ++total; }
		for (std::list<AudioRequest *>::const_iterator a = m_audioRequests.begin(); a != m_audioRequests.end(); ++a)
			if ((*a)->m_usePendingEvent && (*a)->m_pendingEvent->getEventName() == event->getEventName()) { ++requested; ++total; }
		if (event->getAudioEventInfo()->m_control & AC_INTERRUPT)
			if (requested < limit) return FALSE;   // the oldest one is replaced through the kill handle
		if (total < limit) { event->setHandleToKill(0); return FALSE; }
		return TRUE;
	}
	virtual Bool isPlayingLowerPriority(AudioEventRTS *) const { return FALSE; }
	virtual Bool isPlayingAlready(AudioEventRTS *event) const
	{
		const PlayList &l = event->isPositionalAudio() ? m_sounds3D : m_sounds;
		for (PlayList::const_iterator it = l.begin(); it != l.end(); ++it) if ((*it)->ev->getEventName() == event->getEventName()) return TRUE;
		return FALSE;
	}
	virtual Bool isObjectPlayingVoice(UnsignedInt objID) const
	{
		if (objID == 0) return FALSE;
		const PlayList *lists[2] = { &m_sounds, &m_sounds3D };
		for (int i = 0; i < 2; ++i)
			for (PlayList::const_iterator it = lists[i]->begin(); it != lists[i]->end(); ++it)
				if ((*it)->ev->getObjectID() == objID && ((*it)->ev->getAudioEventInfo()->m_type & ST_VOICE)) return TRUE;
		return FALSE;
	}
	virtual void adjustVolumeOfPlayingAudio(AsciiString name, Real vol)
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); ++it)
				if ((*it)->ev->getEventName() == name) { (*it)->ev->setVolume(vol); applyVolume(*it); }
	}
	virtual void removePlayingAudio(AsciiString name)
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); )
				if ((*it)->ev->getEventName() == name) { release(*it); it = lists[i]->erase(it); } else ++it;
	}
	virtual void removeAllDisabledAudio()
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); )
				if ((*it)->ev->getVolume() < 0.0001f) { release(*it); it = lists[i]->erase(it); } else ++it;
	}
	virtual Real getFileLengthMS(AsciiString file) const
	{
		if (file.isEmpty() || !m_deviceOk) return 0.0f;
		std::vector<unsigned char> data;
		if (!const_cast<UmAudioManager *>(this)->readFile(file, data)) return 0.0f;
		return uma_length_ms(&data[0], (unsigned)data.size());
	}
	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS *eventToPlay)
	{
		if (!eventToPlay->getAudioEventInfo()) { getInfoForAudioEvent(eventToPlay); if (!eventToPlay->getAudioEventInfo()) return; }
		AudioEventRTS event = *eventToPlay;
		event.generateFilename(); event.generatePlayInfo();
		std::vector<unsigned char> data;
		if (!readFile(event.getFilename(), data)) return;
		UmaSound *s = uma_create(&data[0], (unsigned)data.size(), 0);
		if (!s) return;
		uma_set_volume(s, event.getVolume() * getVolume(AudioAffect_Speech));
		uma_start(s);
		m_forced.push_back(s);
	}
	virtual void notifyOfAudioCompletion(UnsignedInt, UnsignedInt) {}   // completion is polled in update()
	virtual void setDeviceListenerPosition() {}

private:
	PlayList m_sounds, m_sounds3D, m_streams;
	std::vector<UmaSound *> m_forced;
	std::map<std::string, std::vector<unsigned char> > m_cache;   // small files only (sound effects), keyed by lower-case path
	size_t m_cacheBytes;
	bool m_deviceOk, m_ambientPaused;
	unsigned m_started, m_failed, m_lastLog;
	std::map<std::string, unsigned> m_byName;

	typedef void (UmAudioManager::*PlayFn)(UmPlaying *);
	void forEach(AudioAffect which, PlayFn fn)
	{
		if (which & AudioAffect_Sound) for (PlayList::iterator it = m_sounds.begin(); it != m_sounds.end(); ++it) (this->*fn)(*it);
		if (which & AudioAffect_Sound3D) for (PlayList::iterator it = m_sounds3D.begin(); it != m_sounds3D.end(); ++it) (this->*fn)(*it);
		for (PlayList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
		{
			bool music = (*it)->ev->getAudioEventInfo()->m_soundType == AT_Music;
			if ((music && (which & AudioAffect_Music)) || (!music && (which & AudioAffect_Speech))) (this->*fn)(*it);
		}
	}
	void markStopped(UmPlaying *p) { uma_stop(p->snd); p->stopped = true; }
	void doPause(UmPlaying *p) { uma_pause(p->snd, 1); p->paused = true; }
	void doResume(UmPlaying *p) { uma_pause(p->snd, 0); p->paused = false; }

public:
	// Living World: a world snapshot reload restarts the game's music from the top; the client remembers the track and position and puts them back
	bool umMusicPosition(AsciiString *name, unsigned *ms) const { const UmPlaying *p = findMusic(); if (!p) return false; *name = p->ev->getEventName(); *ms = uma_cursor_ms(p->snd); return true; }
	// the reload restarts the map's music a moment later (or starts another track): wait up to 15 s for a freshly started stream, then put ours back
	void umResumeMusic(const AsciiString &name, unsigned ms) { m_resumeName = name; m_resumeMs = ms; m_resumeUntil = GetTickCount() + 15000; m_resumeSwapped = false; }
	void umResumeTick()
	{
		if (m_resumeName.isEmpty()) return;
		if ((int)(GetTickCount() - m_resumeUntil) > 0) { m_resumeName.clear(); return; }
		const UmPlaying *p = findMusic();
		if (!p) return;
		if (uma_cursor_ms(p->snd) > 3000) return;   // still the old stream: the restart has not come yet
		if (p->ev->getEventName() == m_resumeName)
		{
			uma_seek_ms(p->snd, m_resumeMs);
			printf("[um] audio: music %s resumed at %u ms after the world reload\n", m_resumeName.str(), m_resumeMs);
			m_resumeName.clear();
		}
		else if (!m_resumeSwapped)
		{	// another track started: play the one that was on
			m_resumeSwapped = true;
			TheAudio->removeAudioEvent(AHSV_StopTheMusic);
			AudioEventRTS track(m_resumeName); TheAudio->addAudioEvent(&track);
		}
	}
	unsigned m_resumeUntil; bool m_resumeSwapped;
	AsciiString m_resumeName; unsigned m_resumeMs;
private:
	const UmPlaying *findMusic() const
	{
		for (PlayList::const_iterator it = m_streams.begin(); it != m_streams.end(); ++it)
			if ((*it)->ev->getAudioEventInfo()->m_soundType == AT_Music) return *it;
		return NULL;
	}
	void changeTrack(bool next)
	{
		const UmPlaying *p = findMusic();
		AsciiString cur = p ? p->ev->getEventName() : AsciiString::TheEmptyString;
		TheAudio->removeAudioEvent(AHSV_StopTheMusic);
		AsciiString name = next ? nextTrackName(cur) : prevTrackName(cur);
		AudioEventRTS track(name);
		TheAudio->addAudioEvent(&track);
	}

	// ---- files
	bool readFile(const AsciiString &name, std::vector<unsigned char> &out)
	{
		if (name.isEmpty()) return false;
		std::string key = name.str(); for (size_t i = 0; i < key.size(); ++i) key[i] = (char)tolower((unsigned char)key[i]);
		std::map<std::string, std::vector<unsigned char> >::iterator c = m_cache.find(key);
		if (c != m_cache.end()) { out = c->second; return true; }
		File *f = TheFileSystem->openFile(name.str());
		if (!f) return false;
		Int size = f->size();
		char *buf = f->readEntireAndClose();
		if (!buf || size <= 0) { if (buf) delete[] buf; return false; }
		out.assign((unsigned char *)buf, (unsigned char *)buf + size);
		delete[] buf;
		if (size < 300000 && m_cacheBytes + size < 48u * 1024 * 1024) { m_cache[key] = out; m_cacheBytes += size; }
		return true;
	}

	// ---- volume and pan
	Real typeVolume(const AudioEventInfo *i, bool positional) const
	{
		if (i->m_soundType == AT_Music) return m_musicVolume;
		if (i->m_soundType == AT_Streaming) return m_speechVolume;
		return positional ? m_sound3DVolume : m_soundVolume;
	}
	Real effectiveVolume(AudioEventRTS *event) const
	{
		const AudioEventInfo *info = event->getAudioEventInfo();
		Real volume = event->getVolume() * event->getVolumeShift();
		if (info->m_soundType == AT_Music) return volume * m_musicVolume;
		if (info->m_soundType == AT_Streaming) return volume * m_speechVolume;
		if (!event->isPositionalAudio()) return volume * m_soundVolume;
		volume *= m_sound3DVolume;
		const Coord3D *pos = event->getCurrentPosition();
		if (pos)
		{
			Coord3D d = m_listenerPosition; d.sub(pos);
			Real minD, maxD;
			if (info->m_type & ST_GLOBAL) { minD = m_audioSettings->m_globalMinRange; maxD = m_audioSettings->m_globalMaxRange; }
			else { minD = info->m_minDistance; maxD = info->m_maxDistance; }
			Real dist = d.length();
			if (dist > minD && minD > 0.0f) volume *= 1.0f / (dist / minD);
			if (dist >= maxD) volume = 0.0f;
		}
		return volume;
	}
	float panFor(AudioEventRTS *event) const
	{
		const Coord3D *pos = event->getCurrentPosition();
		if (!pos) return 0.0f;
		Real dx = pos->x - m_listenerPosition.x, dy = pos->y - m_listenerPosition.y;
		Real dist = sqrtf(dx * dx + dy * dy);
		if (dist < 1.0f) return 0.0f;
		Real fx = m_listenerOrientation.x, fy = m_listenerOrientation.y;     // forward; right-hand side is (fy, -fx)
		Real fl = sqrtf(fx * fx + fy * fy); if (fl < 0.0001f) return 0.0f;
		Real p = ((dx * fy - dy * fx) / (dist * fl)) * 0.8f;
		return p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p);
	}
	void applyVolume(UmPlaying *p)
	{
		float v = (p->kind == PK_3D || p->kind == PK_Sample) && p->ev->isPositionalAudio() ? effectiveVolume(p->ev) : effectiveVolume(p->ev);
		uma_set_volume(p->snd, v);
		if (p->kind == PK_3D) uma_set_pan(p->snd, panFor(p->ev));
	}

	// ---- playback
	bool startPortion(UmPlaying *p)
	{
		AudioEventRTS *ev = p->ev;
		AsciiString file;
		switch (ev->getNextPlayPortion())
		{
			case PP_Attack: file = ev->getAttackFilename(); break;
			case PP_Sound: file = ev->getFilename(); break;
			case PP_Decay: file = ev->getDecayFilename(); break;
			default: return false;
		}
		if (p->snd) { uma_destroy(p->snd); p->snd = NULL; }
		if (!m_deviceOk) return false;
		std::vector<unsigned char> data;
		if (file.isEmpty()) return false; if (!readFile(file, data)) { ++m_failed; if (m_failed <= 20) printf("[um] audio: cannot read '%s'\n", file.str()); return false; }
		p->snd = uma_create(&data[0], (unsigned)data.size(), 0);
		if (!p->snd) { ++m_failed; if (m_failed <= 20) printf("[um] audio: cannot decode '%s'\n", file.str()); return false; }
		float pitch = ev->getPitchShift(); if (pitch > 0.0f) uma_set_pitch(p->snd, pitch);
		applyVolume(p);
		uma_start(p->snd);
		++m_started;
		if (getenv("UM_AUDIO_TRACE")) ++m_byName[std::string(ev->getEventName().str()) + " <- " + file.str()];
		return true;
	}
	void release(UmPlaying *p)
	{
		if (p->ev->getAudioEventInfo() && p->ev->getAudioEventInfo()->m_soundType == AT_SoundEffect && m_sound)
		{
			if (p->kind == PK_3D) m_sound->notifyOf3DSampleCompletion(); else m_sound->notifyOf2DSampleCompletion();
		}
		if (p->snd) uma_destroy(p->snd);
		if (p->cleanupEvent) releaseAudioEventRTS(p->ev);
		delete p;
	}
	void stopAllNow()
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i) { for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); ++it) release(*it); lists[i]->clear(); }
		for (size_t i = 0; i < m_forced.size(); ++i) uma_destroy(m_forced[i]);
		m_forced.clear();
	}

	void playAudioEvent(AudioEventRTS *event)
	{
		const AudioEventInfo *info = event->getAudioEventInfo();
		if (!info) { releaseAudioEventRTS(event); return; }
		AudioHandle kill = event->getHandleToKill();
		bool stream = info->m_soundType == AT_Music || info->m_soundType == AT_Streaming;
		bool pos = !stream && event->isPositionalAudio();
		PlayList &list = stream ? m_streams : (pos ? m_sounds3D : m_sounds);

		if (stream && info->m_soundType == AT_Streaming && event->getUninterruptable())
		{
			for (PlayList::iterator it = m_streams.begin(); it != m_streams.end(); )
				if ((*it)->ev->getAudioEventInfo()->m_soundType == AT_Streaming) { release(*it); it = m_streams.erase(it); } else ++it;
		}
		bool replaced = false;
		if (kill)
			for (PlayList::iterator it = list.begin(); it != list.end(); ++it)
				if ((*it)->ev->getPlayingHandle() == kill) { release(*it); list.erase(it); replaced = true; break; }
		if (kill && !replaced) { releaseAudioEventRTS(event); return; }     // the sound to replace is already gone

		UmPlaying *p = new UmPlaying;
		p->ev = event; p->kind = stream ? PK_Stream : (pos ? PK_3D : PK_Sample);
		if (!startPortion(p)) { release(p); return; }
		list.push_back(p);
		if (info->m_soundType == AT_SoundEffect && m_sound) { if (pos) m_sound->notifyOf3DSampleStart(); else m_sound->notifyOf2DSampleStart(); }
		if (stream && info->m_soundType == AT_Streaming && event->getUninterruptable()) setDisallowSpeech(TRUE);
	}
	void stopAudioEvent(AudioHandle handle)
	{
		if (handle == AHSV_StopTheMusic || handle == AHSV_StopTheMusicFade)
		{
			for (PlayList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
				if ((*it)->ev->getAudioEventInfo()->m_soundType == AT_Music) { release(*it); m_streams.erase(it); break; }
			return;
		}
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); ++it)
				if ((*it)->ev->getPlayingHandle() == handle) { (*it)->requestStop = true; uma_stop((*it)->snd); (*it)->stopped = true; return; }
	}
	void processRequestList()
	{
		for (std::list<AudioRequest *>::iterator it = m_audioRequests.begin(); it != m_audioRequests.end(); )
		{
			AudioRequest *req = *it;
			if (req->m_usePendingEvent && req->m_pendingEvent->getDelay() >= MSEC_PER_LOGICFRAME_REAL)
			{
				req->m_pendingEvent->decrementDelay(MSEC_PER_LOGICFRAME_REAL);
				req->m_requiresCheckForSample = true;
				++it; continue;
			}
			bool ok = true;
			if (req->m_requiresCheckForSample && req->m_usePendingEvent)
			{
				if (!req->m_pendingEvent->getAudioEventInfo()) getInfoForAudioEvent(req->m_pendingEvent);
				if (req->m_pendingEvent->getAudioEventInfo()->m_type != AT_SoundEffect) ok = true;
				else ok = m_sound->canPlayNow(req->m_pendingEvent) != 0;
			}
			if (req->m_request == AR_Play && req->m_usePendingEvent)
			{
				if (ok && !m_ambientPaused) playAudioEvent(req->m_pendingEvent); else releaseAudioEventRTS(req->m_pendingEvent);
			}
			else if (req->m_request == AR_Stop) stopAudioEvent(req->m_handleToInteractOn);
			else if (req->m_request == AR_Pause) { /* pausing a single event is not used by the shared code */ }
			req->deleteInstance();
			it = m_audioRequests.erase(it);
		}
	}
	// a non-looping file reached its end: loops, attack/sound/decay portions, music restart
	void completed(UmPlaying *p)
	{
		AudioEventRTS *ev = p->ev; const AudioEventInfo *info = ev->getAudioEventInfo();
		if (getDisallowSpeech() && info->m_soundType == AT_Streaming) setDisallowSpeech(FALSE);
		if (p->kind == PK_Stream && info->m_soundType == AT_Music) { ++p->loopsDone; uma_restart(p->snd); return; }
		if (info->m_control & AC_LOOP)
		{
			if (ev->getNextPlayPortion() == PP_Attack) ev->setNextPlayPortion(PP_Sound);
			if (ev->getNextPlayPortion() == PP_Sound)
			{
				ev->decreaseLoopCount();
				if (!p->requestStop && ev->hasMoreLoops())
				{
					ev->generateFilename();
					if (ev->getDelay() > MSEC_PER_LOGICFRAME_REAL)
					{
						// wait out the delay as a new request; this entry is finished but must not free the event
						p->cleanupEvent = false; p->stopped = true;
						AudioRequest *req = allocateAudioRequest(true);
						req->m_pendingEvent = ev; req->m_request = AR_Play; req->m_requiresCheckForSample = true;
						appendAudioRequest(req);
						return;
					}
					if (startPortion(p)) return;
				}
			}
		}
		ev->advanceNextPlayPortion();
		if (ev->getNextPlayPortion() != PP_Done && !p->requestStop && startPortion(p)) return;
		p->stopped = true;
	}
	void processPlayingList()
	{
		PlayList *lists[3] = { &m_sounds, &m_sounds3D, &m_streams };
		for (int i = 0; i < 3; ++i)
		{
			for (PlayList::iterator it = lists[i]->begin(); it != lists[i]->end(); )
			{
				UmPlaying *p = *it;
				if (!p->stopped && !p->paused && uma_finished(p->snd)) completed(p);
				if (p->stopped) { release(p); it = lists[i]->erase(it); continue; }
				if (m_volumeHasChanged || p->kind == PK_3D)
				{
					if (p->kind == PK_3D)
					{
						if (p->ev->isDead() || !p->ev->getCurrentPosition()) { release(p); it = lists[i]->erase(it); continue; }
						bool always = (p->ev->getAudioEventInfo()->m_type & ST_GLOBAL) || p->ev->getAudioEventInfo()->m_priority == AP_CRITICAL;
						if (!always && effectiveVolume(p->ev) / (m_sound3DVolume > 0.0f ? m_soundVolume : 1.0f) < m_audioSettings->m_minVolume) { release(p); it = lists[i]->erase(it); continue; }
					}
					applyVolume(p);
				}
				++it;
			}
		}
		m_volumeHasChanged = false;
		for (size_t i = 0; i < m_forced.size(); )
			if (uma_finished(m_forced[i])) { uma_destroy(m_forced[i]); m_forced.erase(m_forced.begin() + i); } else ++i;
	}
	void logStats()
	{
		unsigned now = (unsigned)GetTickCount();
		if (now - m_lastLog < 30000) return;
		m_lastLog = now;
		const UmPlaying *m = findMusic();
		printf("[um] audio: %u sounds started, %u failed to load; playing %u sfx, %u 3D, %u streams%s%s (music position %u ms)\n", m_started, m_failed,
			(unsigned)m_sounds.size(), (unsigned)m_sounds3D.size(), (unsigned)m_streams.size(), m ? ", music: " : "", m ? m->ev->getEventName().str() : "", m ? uma_cursor_ms(m->snd) : 0);
		if (getenv("UM_AUDIO_TRACE")) { std::string top; for (int n = 0; n < 6 && !m_byName.empty(); ++n) { std::map<std::string, unsigned>::iterator best = m_byName.begin(); for (std::map<std::string, unsigned>::iterator i = m_byName.begin(); i != m_byName.end(); ++i) if (i->second > best->second) best = i; char b[300]; sprintf(b, "[um] audio top: %u x %s\n", best->second, best->first.c_str()); top += b; m_byName.erase(best); } fputs(top.c_str(), stdout); m_byName.clear(); }
		fflush(stdout);
	}
};

static UmAudioManager *gUmAudio = NULL;
AudioManager *UmCreateRealAudio()
{
	UmAudioManager *a = new UmAudioManager;
	gUmAudio = a;
	return a;
}
bool UmMusicSave(AsciiString *name, unsigned *ms) { return gUmAudio && gUmAudio->umMusicPosition(name, ms); }
void UmMusicResume(const AsciiString &name, unsigned ms) { if (gUmAudio && !name.isEmpty()) gUmAudio->umResumeMusic(name, ms); }
