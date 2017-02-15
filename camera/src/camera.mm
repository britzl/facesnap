#include "camera_private.h"
#include <AVFoundation/AVFoundation.h>

#define DLIB_LOG_DOMAIN "CAMERAEXT"
#include <dmsdk/dlib/log.h>

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX)

// http://easynativeextensions.com/camera-tutorial-part-4-connect-to-the-camera-in-objective-c/
// https://developer.apple.com/library/content/qa/qa1702/_index.html
// http://stackoverflow.com/questions/19422322/method-to-find-devices-camera-resolution-ios
// http://stackoverflow.com/a/32047525


@interface CameraCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
	@private AVCaptureSession* 			m_captureSession;
	@private AVCaptureDevice*			m_camera;
	@private AVCaptureDeviceInput* 		m_cameraInput;
	@private AVCaptureVideoDataOutput*	m_videoOutput;
	@private dispatch_queue_t 			m_Queue;
	@public  CMVideoDimensions 			m_Size;
}
@end

struct IOSCamera
{
	CameraCaptureDelegate* m_Delegate;
	dmBuffer::HBuffer m_VideoBuffer;
	// TODO: Support audio buffers

	IOSCamera() : m_Delegate(0)
	{
	}
};

IOSCamera g_Camera;

@implementation CameraCaptureDelegate


- ( id ) init
{
    self = [ super init ];
    m_captureSession    = NULL;
    m_camera            = NULL;
    m_cameraInput       = NULL;
    m_videoOutput       = NULL;
    return self;
}


- ( void ) onVideoError: ( NSString * ) error
{
    printf("CAMERA HAS ERROR\n");
    NSLog(@"%@",error);
}

- ( void ) onVideoStart: ( NSNotification * ) note
{
    // This callback has done its job, now disconnect it
    //[ [ NSNotificationCenter defaultCenter ] removeObserver: self
    //                                                   name: AVCaptureSessionDidStartRunningNotification
    //                                                 object: m_CaptureSession ];
 
    // Now send an event to ActionScript
    //sendMessage( @"CAMERA_STARTED_EVENT", @"" );
    printf("CAMERA HAS STARTED\n");
}

- ( void ) onVideoStop: ( NSNotification * ) note
{
    printf("CAMERA HAS STOPPED\n");
}


// Delegate routine that is called when a sample buffer was written
- (void)captureOutput:(AVCaptureOutput *)captureOutput
         didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection
{
    if( captureOutput == m_videoOutput )
    {
	    uint8_t* data = 0;
	    uint32_t datasize = 0;
	    dmBuffer::GetBytes(g_Camera.m_VideoBuffer, (void**)&data, &datasize);

	    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
	    CVPixelBufferLockBaseAddress(imageBuffer,0);

	    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
	    size_t width = CVPixelBufferGetWidth(imageBuffer);
	    size_t height = CVPixelBufferGetHeight(imageBuffer);

	    if( width != g_Camera.m_Delegate->m_Size.width || height != g_Camera.m_Delegate->m_Size.height )
	    {
	    	CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
	    	return;
	    }

	    uint8_t* pixels = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);

	    for( int y = 0; y < height; ++y )
		{
	    	for( int x = 0; x < width; ++x )
	    	{
	    		// RGB < BGR(A)
#if defined(DM_PLATFORM_IOS)
                // Flip X
	    		data[y*width*3 + x*3 + 2] = pixels[y * bytesPerRow + bytesPerRow - (x+1) * 4 + 0];
	    		data[y*width*3 + x*3 + 1] = pixels[y * bytesPerRow + bytesPerRow - (x+1) * 4 + 1];
	    		data[y*width*3 + x*3 + 0] = pixels[y * bytesPerRow + bytesPerRow - (x+1) * 4 + 2];
#else
                // Flip X + Y

                data[y*width*3 + x*3 + 2] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 0];
                data[y*width*3 + x*3 + 1] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 1];
                data[y*width*3 + x*3 + 0] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 2];
#endif

	    	}
		}

	    CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
	    dmBuffer::ValidateBuffer(g_Camera.m_VideoBuffer);
	}
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput 
  didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer 
       fromConnection:(AVCaptureConnection *)connection
{

	NSLog(@"DROPPING FRAME!!!");
}


- ( BOOL ) findCamera: (AVCaptureDevicePosition) cameraPosition
{
    // 0. Make sure we initialize our camera pointer:
    m_camera = NULL;

    // 1. Get a list of available devices:
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray * devices = [ AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo ];

    // 2. Iterate through the device array and if a device is a camera, check if it's the one we want:
    for ( AVCaptureDevice * device in devices )
    {
        if ( cameraPosition == [ device position ] )
        {
            m_camera = device;
        }
    }

    // 3. Set a frame rate for the camera:
    if ( NULL != m_camera )
    {
#if defined(DM_PLATFORM_IOS)
        // We firt need to lock the camera, so noone else can mess with its configuration:
        if ( [ m_camera lockForConfiguration: NULL ] )
        {
            // Set a minimum frame rate of 10 frames per second
            [ m_camera setActiveVideoMinFrameDuration: CMTimeMake( 1, 10 ) ];
            // and a maximum of 30 frames per second
            [ m_camera setActiveVideoMaxFrameDuration: CMTimeMake( 1, 30 ) ];

            [ m_camera unlockForConfiguration ];
        }
#endif
    }

    // 4. If we've found the camera we want, return true
    return ( NULL != m_camera );
}

- ( BOOL ) attachCameraToCaptureSession
{
    // 0. Assume we've found the camera and set up the session first:
    assert( NULL != m_camera );
    assert( NULL != m_captureSession );

    // 1. Initialize the camera input
    m_cameraInput = NULL;

    // 2. Request a camera input from the camera
    NSError * error = NULL;
    m_cameraInput = [ AVCaptureDeviceInput deviceInputWithDevice: m_camera error: &error ];

    // 2.1. Check if we've got any errors
    if ( NULL != error )
    {
        // TODO: send an error event to ActionScript
        return false;
    }

    // 3. We've got the input from the camera, now attach it to the capture session:
    if ( [ m_captureSession canAddInput: m_cameraInput ] )
    {
        [ m_captureSession addInput: m_cameraInput ];
    }
    else
    {
        // TODO: send an error event to ActionScript
        return false;
    }

    // 4. Done, the attaching was successful, return true to signal that
    return true;
}

- ( void ) setupVideoOutput
{
    // 1. Create the video data output
    m_videoOutput = [ [ AVCaptureVideoDataOutput alloc ] init ];

    // 2. Create a queue for capturing video frames
    dispatch_queue_t captureQueue = dispatch_queue_create( "captureQueue", DISPATCH_QUEUE_SERIAL );

    // 3. Use the AVCaptureVideoDataOutputSampleBufferDelegate capabilities of CameraDelegate:
    [ m_videoOutput setSampleBufferDelegate: self queue: captureQueue ];

    // 4. Set up the video output
    // 4.1. Do we care about missing frames?
    m_videoOutput.alwaysDiscardsLateVideoFrames = NO;

    // 4.2. We want the frames in some RGB format, which is what ActionScript can deal with
    NSNumber * framePixelFormat = [ NSNumber numberWithInt: kCVPixelFormatType_32BGRA ];
    m_videoOutput.videoSettings = [ NSDictionary dictionaryWithObject: framePixelFormat
                                                               forKey: ( id ) kCVPixelBufferPixelFormatTypeKey ];

    // 5. Add the video data output to the capture session
    [ m_captureSession addOutput: m_videoOutput ];
}


- ( BOOL ) startCamera: (AVCaptureDevicePosition) cameraPosition
{
    // 1. Find the back camera
    if ( ![ self findCamera: cameraPosition ] )
    {
        return false;
    }

    //2. Make sure we have a capture session
    if ( NULL == m_captureSession )
    {
        m_captureSession = [ [ AVCaptureSession alloc ] init ];
    }

    // 3. Choose a preset for the session.
    // Optional TODO: You can parameterize this and set it in ActionScript.
    //NSString * cameraResolutionPreset = AVCaptureSessionPreset640x480;
    NSString * cameraResolutionPreset = AVCaptureSessionPreset1280x720;

    // 4. Check if the preset is supported on the device by asking the capture session:
    if ( ![ m_captureSession canSetSessionPreset: cameraResolutionPreset ] )
    {
        // Optional TODO: Send an error event to ActionScript
        return false;
    }

    // 4.1. The preset is OK, now set up the capture session to use it
    [ m_captureSession setSessionPreset: cameraResolutionPreset ];

    // 5. Plug camera and capture sesiossion together
    [ self attachCameraToCaptureSession ];

    // 6. Add the video output
    [ self setupVideoOutput ];

    // 7. Set up a callback, so we are notified when the camera actually starts
    [ [ NSNotificationCenter defaultCenter ] addObserver: self
                                                selector: @selector( onVideoStart: )
                                                    name: AVCaptureSessionDidStartRunningNotification
                                                  object: m_captureSession ];

    // 8. 3, 2, 1, 0... Start!
    [ m_captureSession startRunning ];

    // Note: Returning true from this function only means that setting up went OK.
    // It doesn't mean that the camera has started yet.
    // We get notified about the camera having started in the videoCameraStarted() callback.
    return true;
}



- ( BOOL ) stopCamera
{
    BOOL isRunning = [m_captureSession isRunning];
    if( isRunning )
    {
        [m_captureSession stopRunning ];
    }
    return isRunning;
}

@end


CMVideoDimensions _CameraPlatform_GetSize(AVCaptureDevicePosition cameraPosition)
{
	// http://stackoverflow.com/a/32047525

    CMVideoDimensions max_resolution;
    max_resolution.width = 0;
    max_resolution.height = 0;

    AVCaptureDevice *captureDevice = nil;

    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    for (AVCaptureDevice *device in devices) {
        
        const char* position = "unspecified";
        if( device.position == AVCaptureDevicePositionBack )
            position = "back";
        else if (device.position == AVCaptureDevicePositionFront )
            position = "front";

        NSString* localizedName = [NSString stringWithFormat:@"%@,  position: %s", device.localizedName, position];
        NSLog(@"%@", localizedName);

        if ([device position] == cameraPosition) {
            captureDevice = device;
            break;
        }
    }
    if (captureDevice == nil) {
        return max_resolution;
    }

    NSArray* availFormats=captureDevice.formats;

    for (AVCaptureDeviceFormat* format in availFormats) {
#if defined(DM_PLATFORM_IOS)
        CMVideoDimensions resolution = format.highResolutionStillImageDimensions;
#else
        CMVideoDimensions resolution = CMVideoFormatDescriptionGetDimensions((CMVideoFormatDescriptionRef)[format formatDescription]);
#endif
        int w = resolution.width;
        int h = resolution.height;
        if ((w * h) > (max_resolution.width * max_resolution.height)) {
            max_resolution.width = w;
            max_resolution.height = h;
        }
    }

	return max_resolution;
}


int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CameraInfo& outparams)
{

	if(g_Camera.m_Delegate == 0)
	{
		g_Camera.m_Delegate	= [[CameraCaptureDelegate alloc] init];
	}

    AVCaptureDevicePosition cameraposition = AVCaptureDevicePositionUnspecified;
#if defined(DM_PLATFORM_IOS)
    if( type == CAMERA_TYPE_BACK )
        cameraposition = AVCaptureDevicePositionBack;
    else if( type == CAMERA_TYPE_FRONT )
        cameraposition = AVCaptureDevicePositionFront;
#endif
	CMVideoDimensions dimensions = _CameraPlatform_GetSize(cameraposition);

	dimensions.width = 1280;
	dimensions.height = 720;

	g_Camera.m_Delegate->m_Size = dimensions;

    outparams.m_Width = (uint32_t)dimensions.width;
    outparams.m_Height = (uint32_t)dimensions.height;

    uint32_t size = outparams.m_Width * outparams.m_Width;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };

    dmBuffer::Allocate(size, streams_decl, 1, buffer);

	g_Camera.m_VideoBuffer = *buffer;

	BOOL started = [g_Camera.m_Delegate startCamera: cameraposition];

    return started ? 1 : 0;
}

int CameraPlatform_StopCapture()
{
	if(g_Camera.m_Delegate != 0)
	{
        [g_Camera.m_Delegate stopCamera];
    	//[g_Camera.m_Delegate release];
	}
	return 1;
}

#else

int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CameraInfo& outparams)
{
	return 0;
}

int CameraPlatform_StopCapture()
{
	return 0;
}

#endif // DM_PLATFORM_IOS
