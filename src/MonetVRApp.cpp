#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/gl.h"
//#include "cinder/ImageIo.h"
#include "cinder/Capture.h"
#include "cinder/Log.h"
#include "cinder/MotionManager.h"

#include "CinderOpenCv.h"

using namespace ci;
using namespace ci::app;
using namespace std;

static void printDevices()
{
    for ( const auto &device : Capture::getDevices() ) {
        console() << "Device: " << device->getName() << " "
#if defined( CINDER_COCOA_TOUCH )
        << ( device->isFrontFacing() ? "Front" : "Rear" ) << "-facing"
#endif
        << endl;
    }
}

class MonetVRApp : public App {
public:
    void setup() override;
    void update() override;
    
    void draw() override;
    
    CaptureRef      mCapture;
    
    gl::TextureRef  mTexture;
    gl::GlslProgRef mGlsl;
    
    mat4            mModelMatrix;
    CameraPersp     mCam;
};

void MonetVRApp::setup()
{
    printDevices();
    
    try {
        mCapture = Capture::create( 640, 480 );
        mCapture->start();
    }
    catch ( ci::Exception &exc ) {
        CI_LOG_EXCEPTION( "Failed to init capture ", exc );
    }
    
    CI_LOG_V( "gyro available: " << MotionManager::isGyroAvailable() );
    
    MotionManager::enable( 60.0f/*, MotionManager::SensorMode::Accelerometer*/ );
    
    mCam.setPerspective( 60, getWindowAspectRatio(), 1, 1000 );
    mCam.lookAt( vec3( 0, 0, 3 ), vec3( 0 ) );
    
    // setup shader
    try {
        mGlsl = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "monet.vert" ) )
                                     .fragment( loadAsset( "monet.frag" ) )
                                     );
    }
    catch( gl::GlslProgCompileExc ex ) {
        CI_LOG_EXCEPTION("glsl", ex);
        quit();
    }
}

void MonetVRApp::update()
{
    if ( mCapture && mCapture->checkNewFrame() ) {
        if ( ! mTexture ) {
            // Capture images come back as top-down, and it's more efficient to keep them that way
            mTexture = gl::Texture::create( *mCapture->getSurface(), gl::Texture::Format().loadTopDown() );
        }
        else {
            mTexture->update( *mCapture->getSurface() );
        }
    }
    
    if ( MotionManager::isEnabled() )
    {
        mModelMatrix = inverse( MotionManager::getRotationMatrix() );
    }
    
    
}

void MonetVRApp::draw()
{
    gl::clear();
    
    gl::disableDepthWrite();
    gl::setMatricesWindow(this->getWindowSize());
    if ( mTexture) {
        gl::ScopedModelMatrix modelScope;
        gl::ScopedGlslProg glslProg( mGlsl );
        
        mTexture->bind();
        mGlsl->uniform( "uTex0", 0 );

#if defined( CINDER_COCOA_TOUCH )
        // change iphone to landscape orientation
        gl::rotate( M_PI / 2 );
        gl::translate( 0, - getWindowWidth() );
        
        Rectf flippedBounds( 0, 0, getWindowHeight(), getWindowWidth() );
//        gl::draw( mTexture, flippedBounds );
        gl::drawSolidRect( flippedBounds );

#else
        gl::draw( mTexture );
#endif
    }
    
    gl::enableDepthRead();
    gl::enableDepthWrite();

    gl::setMatrices( mCam );
    gl::multModelMatrix( mModelMatrix );
    
    gl::drawCoordinateFrame();
    gl::drawColorCube( vec3(), vec3( 0.3 ) );
}

CINDER_APP( MonetVRApp, RendererGl(RendererGl::Options().version(2, 0) ))
