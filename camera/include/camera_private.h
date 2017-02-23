#pragma once

#include <dmsdk/dlib/buffer.h>

enum CameraType
{
	CAMERA_TYPE_FRONT, // Selfie
	CAMERA_TYPE_BACK
};

struct CameraInfo
{
	uint32_t m_Width;
	uint32_t m_Height;
};

extern int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CameraInfo& outparams);
extern int CameraPlatform_StopCapture();

