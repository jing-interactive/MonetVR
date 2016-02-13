#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Capture.h"
#include "cinder/Log.h"
#include "cinder/MotionManager.h"
#include "AssetManager.h"

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
    enum EyeType
    {
        MonoEye,
        LeftEye,
        RightEye,
        EyeCount,
    };
    
    bool mIsStereo = true;
    
    void setup() override;
    void update() override;
    
    void draw() override;
    
    void drawFromEye(EyeType type);
    void setupEyeInfos();
    
    CaptureRef      mCapture;
    gl::TextureRef  mCaptureTexture;
    ivec2           mCaptureSize;
    
    gl::GlslProgRef     mGlsl;
    
    mat4            mDeviceRotation;
    
    struct
    {
        CameraPersp         camMatrix;
        gl::VertBatchRef   videoPlane;
        ivec4           viewport;
        ivec2           windowSize;
    } mEyeInfos[EyeCount];
};

void MonetVRApp::setupEyeInfos()
{
    int eyeCount = 0;
    for (auto& eyeInfo : mEyeInfos)
    {
        float w = toPixels(getWindowWidth());
        float h = toPixels(getWindowHeight());
        
        if (eyeCount == MonoEye)
        {
            eyeInfo.viewport = {0,0,w,h};
            eyeInfo.windowSize = {w,h};
        }
        else if (eyeCount == LeftEye)
        {
            eyeInfo.viewport = {0,0,w/2,h};
            eyeInfo.windowSize = {w/2,h};
        }
        else
        {
            eyeInfo.viewport = {w/2,0,w/2,h};
            eyeInfo.windowSize = {w/2,h};
        }
        
        eyeInfo.camMatrix.setPerspective( 60, getWindowAspectRatio(), 1, 1000 );
        eyeInfo.camMatrix.lookAt( vec3( 0, 0, 3 ), vec3( 0 ) );
        
        eyeInfo.videoPlane = gl::VertBatch::create();
        
        for (int y=0;y<mCaptureSize.y;y++)
        {
            float v = y/(float)mCaptureSize.y;
            for (int x=0;x<mCaptureSize.x;x++)
            {
                float u = x/(float)mCaptureSize.x;
                eyeInfo.videoPlane->vertex(x, y);
                eyeInfo.videoPlane->texCoord(u, v);
            }
        }
        
        eyeCount++;
    }
}

void MonetVRApp::setup()
{
    printDevices();
    
    try {
        mCapture = Capture::create( 640, 480 );
        mCapture->start();
        mCaptureSize = mCapture->getSize();
    }
    catch ( ci::Exception &exc ) {
        CI_LOG_EXCEPTION( "Failed to init capture ", exc );
    }
    
    CI_LOG_V( "gyro available: " << MotionManager::isGyroAvailable() );
    
    MotionManager::enable( 1000.0f/*, MotionManager::SensorMode::Accelerometer*/ );
    
    // setup shader
    mGlsl = am::glslProg("shaders/monet.vert", "shaders/monet.frag");
    
    getWindow()->getSignalMouseDown().connect( [this]( MouseEvent event ) {
        mIsStereo = !mIsStereo;
    });

    setupEyeInfos();
}

void MonetVRApp::update()
{
    if ( mCapture && mCapture->checkNewFrame() ) {
        if ( ! mCaptureTexture ) {
            // Capture images come back as top-down, and it's more efficient to keep them that way
            mCaptureTexture = gl::Texture::create( *mCapture->getSurface(), gl::Texture::Format().loadTopDown() );
        }
        else {
            mCaptureTexture->update( *mCapture->getSurface() );
        }
    }
    
    if ( MotionManager::isEnabled() )
    {
        mDeviceRotation = inverse( MotionManager::getRotationMatrix() );
    }
    
}

void MonetVRApp::draw()
{
    gl::clear();
    
    if (mIsStereo)
    {
        drawFromEye(LeftEye);
        drawFromEye(RightEye);
    }
    else
    {
        drawFromEye(MonoEye);
    }
}

void MonetVRApp::drawFromEye(EyeType type)
{
    auto& eyeInfo = mEyeInfos[type];
    
    gl::ScopedViewport scopedViewport(eyeInfo.viewport.x, eyeInfo.viewport.y, eyeInfo.viewport.z, eyeInfo.viewport.w);
    
    gl::disableDepthWrite();
    gl::setMatricesWindow(eyeInfo.windowSize);
    if ( mCaptureTexture) {
        gl::ScopedModelMatrix modelScope;
        gl::ScopedGlslProg glslProg( mGlsl );
        
        mCaptureTexture->bind();
        mGlsl->uniform( "uTex0", 0 );
        
#if defined( CINDER_COCOA_TOUCH )
        // change iphone to landscape orientation
        gl::rotate( M_PI / 2 );
        gl::translate( 0, - getWindowWidth() );
        
        //        Rectf flippedBounds( 0, 0, getWindowHeight(), getWindowWidth() );
        //        gl::draw( mTexture, flippedBounds );
        //        gl::drawSolidRect( flippedBounds );
        eyeInfo.videoPlane->draw();
        
#else
        gl::draw( mTexture );
#endif
    }
    
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    gl::setMatrices( eyeInfo.camMatrix );
    gl::multViewMatrix( mDeviceRotation );
    
    gl::drawCoordinateFrame();
    //    gl::drawColorCube( vec3(), vec3( 0.1 ) );
    
    {
        gl::ScopedModelMatrix modelScope;
        gl::translate(0, 0, -3);
        
        gl::scale(vec3(0.02));
        
        gl::ScopedGlslProg glslProg(gl::getStockShader( gl::ShaderDef().texture().lambert()));
        am::texture("meshes/leprechaun_diffuse.jpg")->bind(0);
        am::texture("meshes/leprechaun_specular.jpg")->bind(1);
        am::texture("meshes/leprechaun_normal.jpg")->bind(2);
        am::texture("meshes/leprechaun_emmisive.png")->bind(3);
        
        gl::draw(am::vboMesh("meshes/leprechaun.msh"));
    }
}

static void prepareSettings( MonetVRApp::Settings *settings )
{
    //    settings->setHighDensityDisplayEnabled(); // try removing this line
    settings->setMultiTouchEnabled( false );
    //
    //    // on iOS we want to make a Window per monitor
    //#if defined( CINDER_COCOA_TOUCH )
    //    for( auto display : Display::getDisplays() )
    //        settings->prepareWindow( Window::Format().display( display ) );
    //#endif
}

CINDER_APP( MonetVRApp, RendererGl(RendererGl::Options().version(2, 0)), prepareSettings )
