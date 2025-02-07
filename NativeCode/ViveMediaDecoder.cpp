﻿//========= Copyright 2015-2019, HTC Corporation. All rights reserved. ===========

#include "ViveMediaDecoder.h"
#include "AVHandler.h"
#include "Logger.h"
#include <stdio.h>
#include <string>
#include <memory>
#include <thread>
#include <list>
#include <cstring>

typedef struct _VideoContext {
	int id = -1;
	std::string path = "";
    std::thread initThread;
    std::shared_ptr<AVHandler> avhandler = nullptr;
	float progressTime = 0.0f;
	float lastUpdateTime = -1.0f;
    bool videoFrameLocked = false;
	bool isContentReady = false;	//	This flag is used to indicate the period that seek over until first data is got.
									//	Usually used for AV sync problem, in pure audio case, it should be discard.
} VideoContext;

std::list<std::shared_ptr<VideoContext>> videoContexts;
typedef std::list<std::shared_ptr<VideoContext>>::iterator VideoContextIter;

bool getVideoContext(int id, std::shared_ptr<VideoContext>& videoCtx) {
	for (VideoContextIter it = videoContexts.begin(); it != videoContexts.end(); it++) {
		if ((*it)->id == id) {
			videoCtx = *it;
			return true;
		}
	}

	LOG("Decoder does not exist. \n");
	return false;
}

void removeVideoContext(int id) {
	for (VideoContextIter it = videoContexts.begin(); it != videoContexts.end(); it++) {
		if ((*it)->id == id) {
			videoContexts.erase(it);
			return;
		}
	}
}

void nativeCleanAll() {
    std::list<int> idList;
    for(auto videoCtx : videoContexts) {
        idList.push_back(videoCtx->id);
    }

    for(int id : idList) {
        removeVideoContext(id);
    }
}

int nativeCreateDecoderAsync(const char* filePath, int& id) {
	LOG("Query available decoder id. \n");

	int newID = 0;
    std::shared_ptr<VideoContext> videoCtx;
	while (getVideoContext(newID, videoCtx)) { newID++; }

	videoCtx = std::make_shared<VideoContext>();
	videoCtx->avhandler = std::make_shared<AVHandler>();
	videoCtx->id = newID;
	id = videoCtx->id;
	videoCtx->path = std::string(filePath);
	videoCtx->isContentReady = false;

	videoCtx->initThread = std::thread([videoCtx]() {
		videoCtx->avhandler->init(videoCtx->path.c_str());
	});

	videoContexts.push_back(videoCtx);

	return 0;
}

//	Synchronized init. Used for thumbnail currently.
int nativeCreateDecoder(const char* filePath, int& id) {
	LOG("Query available decoder id. \n");

	int newID = 0;
    std::shared_ptr<VideoContext> videoCtx;
	while (getVideoContext(newID, videoCtx)) { newID++; }

    videoCtx = std::make_shared<VideoContext>();
    videoCtx->avhandler = std::make_shared<AVHandler>();
    videoCtx->id = newID;
    id = videoCtx->id;
    videoCtx->path = std::string(filePath);
    videoCtx->isContentReady = false;
	videoCtx->avhandler->init(filePath);

	videoContexts.push_back(videoCtx);

	return 0;
}

int nativeGetDecoderState(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx) || videoCtx->avhandler == nullptr) { return -1; }
		
	return videoCtx->avhandler->getDecoderState();
}

bool nativeStartDecoding(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx) || videoCtx->avhandler == nullptr) { return false; }

	if (videoCtx->initThread.joinable()) {
		videoCtx->initThread.join();
	}

	auto avhandler = videoCtx->avhandler;
	if (avhandler->getDecoderState() >= AVHandler::DecoderState::INITIALIZED) {
		avhandler->startDecoding();
	}

	if (!avhandler->getVideoInfo().isEnabled) {
		videoCtx->isContentReady = true;
	}

	return true;
}

void nativeDestroyDecoder(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	if (videoCtx->initThread.joinable()) {
		videoCtx->initThread.join();
	}

	videoCtx->avhandler = nullptr;

	videoCtx->path.clear();
	videoCtx->progressTime = 0.0f;
	videoCtx->lastUpdateTime = 0.0f;

	videoCtx->isContentReady = false;
	removeVideoContext(videoCtx->id);
	videoCtx->id = -1;
}

//	Video
bool nativeIsVideoEnabled(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }

	if (videoCtx->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return false;
	}

	bool ret = videoCtx->avhandler->getVideoInfo().isEnabled;
	LOG("nativeIsVideoEnabled: %s \n", ret ? "true" : "false");
	return ret;
}

void nativeGetVideoFormat(int id, int& width, int& height, float& totalTime) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	if (videoCtx->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	IDecoder::VideoInfo videoInfo = videoCtx->avhandler->getVideoInfo();
	width = videoInfo.width;
	height = videoInfo.height;
	totalTime = (float)(videoInfo.totalTime);
}

void nativeSetVideoTime(int id, float currentTime) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	videoCtx->progressTime = currentTime;
}

bool nativeIsAudioEnabled(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }

	if (videoCtx->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return false;
	}

	bool ret = videoCtx->avhandler->getAudioInfo().isEnabled;
	LOG("nativeIsAudioEnabled: %s \n", ret ? "true" : "false");
	return ret;
}

void nativeGetAudioFormat(int id, int& channel, int& frequency, float& totalTime) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	if (videoCtx->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	IDecoder::AudioInfo audioInfo = videoCtx->avhandler->getAudioInfo();
	channel = audioInfo.channels;
	frequency = audioInfo.sampleRate;
	totalTime = (float)(audioInfo.totalTime);
}

float nativeGetAudioData(int id, unsigned char** audioData, int& frameSize) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return -1.0f; }

	return (float) (videoCtx->avhandler->getAudioFrame(audioData, frameSize));
}

void nativeFreeAudioData(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }
	
	videoCtx->avhandler->freeAudioFrame();
}

void nativeSetSeekTime(int id, float sec) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	if (videoCtx->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	LOG("nativeSetSeekTime %f. \n", sec);
	videoCtx->avhandler->setSeekTime(sec);
	if (!videoCtx->avhandler->getVideoInfo().isEnabled) {
		videoCtx->isContentReady = true;
	} else {
		videoCtx->isContentReady = false;
	}
}

bool nativeIsSeekOver(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }
	
	return !(videoCtx->avhandler->getDecoderState() == AVHandler::DecoderState::SEEK);
}

bool nativeIsVideoBufferFull(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }

	return videoCtx->avhandler->isVideoBufferFull();
}

bool nativeIsVideoBufferEmpty(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }
	
	return videoCtx->avhandler->isVideoBufferEmpty();
}

int nativeGetMetaData(const char* filePath, char*** key, char*** value) {
    std::unique_ptr<AVHandler> avHandler = std::make_unique<AVHandler>();
	avHandler->init(filePath);

	char** metaKey = nullptr;
	char** metaValue = nullptr;
	int metaCount = avHandler->getMetaData(metaKey, metaValue);

	*key = (char**)malloc(sizeof(char*) * metaCount);
	*value = (char**)malloc(sizeof(char*) * metaCount);

	for (int i = 0; i < metaCount; i++) {
		(*key)[i] = (char*)malloc(strlen(metaKey[i]) + 1);
		(*value)[i] = (char*)malloc(strlen(metaValue[i]) + 1);
		strcpy_s((*key)[i], strlen(metaKey[i]) + 1, metaKey[i]);
		strcpy_s((*value)[i], strlen(metaValue[i]) + 1, metaValue[i]);
	}

	free(metaKey);
	free(metaValue);

	return metaCount;
}

bool nativeIsContentReady(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return false; }

	return videoCtx->isContentReady;
}

void nativeSetVideoEnable(int id, bool isEnable) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	videoCtx->avhandler->setVideoEnable(isEnable);
}

void nativeSetAudioEnable(int id, bool isEnable) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	videoCtx->avhandler->setAudioEnable(isEnable);
}

void nativeSetAudioAllChDataEnable(int id, bool isEnable) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx)) { return; }

	videoCtx->avhandler->setAudioAllChDataEnable(isEnable);
}

void nativeGrabVideoFrame(int id, void** frameData, bool& frameReady) {
    frameReady = false;
    std::shared_ptr<VideoContext> videoCtx;
    if (!getVideoContext(id, videoCtx) || videoCtx->avhandler == nullptr) { return; }
    if (videoCtx->videoFrameLocked) {
        LOG("Release last video frame first");
        return;
    }

    AVHandler* localAVHandler = videoCtx->avhandler.get();

    if (localAVHandler != nullptr && localAVHandler->getDecoderState() >= AVHandler::DecoderState::INITIALIZED && localAVHandler->getVideoInfo().isEnabled) {
        double videoDecCurTime = localAVHandler->getVideoInfo().lastTime;
        if (videoDecCurTime <= videoCtx->progressTime) {

            double curFrameTime = localAVHandler->getVideoFrame(frameData);
            if (frameData != nullptr && curFrameTime != -1 && videoCtx->lastUpdateTime != curFrameTime) {
                frameReady = true;
                videoCtx->lastUpdateTime = (float)curFrameTime;
                videoCtx->isContentReady = true;
                videoCtx->videoFrameLocked = true;
            }
        }
    }
}

void nativeReleaseVideoFrame(int id) {
    std::shared_ptr<VideoContext> videoCtx;
    if (!getVideoContext(id, videoCtx) || videoCtx->avhandler == nullptr) { return; }
    videoCtx->avhandler->freeVideoFrame();
    videoCtx->videoFrameLocked = false;
}

bool nativeIsEOF(int id) {
    std::shared_ptr<VideoContext> videoCtx;
	if (!getVideoContext(id, videoCtx) || videoCtx->avhandler == nullptr) { return true; }

	return videoCtx->avhandler->getDecoderState() == AVHandler::DecoderState::DECODE_EOF;
}