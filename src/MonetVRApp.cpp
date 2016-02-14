// https://github.com/momo-the-monster/Cinder-OculusRift/tree/master/src
#include "cinder/app/App.h"
#include "cinder/Log.h"
#include "cinder/ip/Checkerboard.h"

// gl
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// VNM
#include "AssetManager.h"
#include "CaptureHelper.h"
#include "MotionHelper.h"

#include "CinderOpenCv.h"

using namespace ci;
using namespace ci::app;
using namespace std;


class MonetVRApp : public App {
public:
    enum EyeType
    {
        MonoEye,
        LeftEye,
        RightEye,
        EyeCount,
    };
    
    bool mIsStereo = false;
    
    void setup() override;
    void update() override;
    void cleanup() override
    {
        if (mLoadingThread)
            mLoadingThread->join();
    }
    
    void draw() override;
    
    void drawFromEye(EyeType type);
    void setupEyeInfos();
    

    CaptureHelper mCaptureHelper;
    MotionHelper mMotionHelper;
    gl::VboMeshRef mMonster;
    
    void setupLoadingThread();
    shared_ptr<thread>		mLoadingThread;
    
//    gl::GlslProgRef     mGlsl;
    
    gl::BatchRef           mBatch;

    struct
    {
        CameraPersp         camMatrix;
        ivec4           viewport;
        ivec2           windowSize;
    } mEyeInfos[EyeCount];
};

void MonetVRApp::setupLoadingThread()
{
    auto loadFn = [this]
    {
        auto triMesh = am::triMesh("meshes/leprechaun.msh");
        
        auto meshFn = [triMesh, this]
        {
            mMonster = gl::VboMesh::create(*triMesh);
        };
        dispatchAsync(meshFn);
    };
    mLoadingThread = shared_ptr<thread>( new thread( loadFn ) );
}

void MonetVRApp::setupEyeInfos()
{
    int eyeCount = 0;
    for (auto& eyeInfo : mEyeInfos)
    {
        float w = toPixels(getWindowWidth()); // shorter edge
        float h = toPixels(getWindowHeight());
        
        if (eyeCount == MonoEye)
        {
            eyeInfo.viewport = {0,0,w,h};
            eyeInfo.windowSize = {w,h};
        }
        else if (eyeCount == LeftEye)
        {
            eyeInfo.viewport = {0,0,w,h/2};
            eyeInfo.windowSize = {w,h/2};
        }
        else
        {
            eyeInfo.viewport = {0,h/2,w,h/2};
            eyeInfo.windowSize = {w,h/2};
        }
        
        eyeInfo.camMatrix.setPerspective( 60, eyeInfo.windowSize.x / (float)eyeInfo.windowSize.y, 1, 1000 );
        eyeInfo.camMatrix.lookAt( vec3( 0, 0, 10 ), vec3( 0 ) );
        
        eyeCount++;
    }
    
    TriMesh triMesh;
    
    float du = 1.0 / mCaptureHelper.size.x;
    float dv = 1.0 / mCaptureHelper.size.y;
    
    for (float v=0;v<=1-du;v+=dv)
    {
        for (float u=0;u<=1-du;u+=du)
        {
#define UNIT(m,n) \
triMesh.appendPosition(vec3((v+n*dv)*4.8, (u+m*du)*6.4, 0));\
triMesh.appendTexCoord(vec2(1.0 - (u+m*du),  1.0- (v+n*dv)));
            
            /*
             0,1  1,1
             0,0  1,0
             // FrontFace: GL_CCW
             */
            UNIT(0,0);
            UNIT(0,1);
            UNIT(1,0);
            
            UNIT(1,0);
            UNIT(1,1);
            UNIT(0,1);
#undef UNIT
        }
    }
    
    //        auto glsl = gl::getStockShader( gl::ShaderDef().texture());
    triMesh.recalculateNormals();
    auto glsl = am::glslProg("shaders/monet.vert", "shaders/monet.frag");
    mBatch = gl::Batch::create(triMesh, glsl);
}

void MonetVRApp::setup()
{
    CaptureHelper::printDevices();
    
    mCaptureHelper.setup();
    mMotionHelper.setup();
    
    getWindow()->getSignalMouseDown().connect( [this]( MouseEvent event ) {
        mIsStereo = !mIsStereo;
    });

    setupEyeInfos();
    
    setupLoadingThread();
}

void MonetVRApp::update()
{

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
    
    gl::enableDepthRead();
    gl::enableDepthWrite();
//    gl::setMatricesWindow(getWindowSize());
    
    gl::setMatrices( eyeInfo.camMatrix );

    if ( mCaptureHelper.isReady()) {
        gl::ScopedModelMatrix modelScope;
//        gl::ScopedGlslProg glslProg( gl::getStockShader( gl::ShaderDef().texture().color() ));
        
#if 1
        mCaptureHelper.texture->bind();
#else
        am::texture("checkerboard.jpg")->bind(0);
#endif
//        mGlsl->uniform( "uTex0", 0 );
        
#if defined( CINDER_COCOA_TOUCH )
        gl::translate( -2.4, -3.2, 0 ); // TODO: magic number
        gl::scale(vec3(1.0));
        
        mBatch->draw();
        
#else
        gl::draw( mTexture );
#endif
    }

    gl::multViewMatrix( mMotionHelper.deviceRotation );
    
    gl::drawCoordinateFrame();
    //    gl::drawColorCube( vec3(), vec3( 0.1 ) );
    
    if (mMonster)
    {
        gl::ScopedModelMatrix modelScope;
        gl::translate(0, 0, 0);
        
        gl::scale(vec3(0.05));
        
        gl::ScopedGlslProg glslProg(gl::getStockShader( gl::ShaderDef().texture().lambert()));
        am::texture("meshes/leprechaun_diffuse.jpg")->bind(0);
#if 0
        am::texture("meshes/leprechaun_specular.jpg")->bind(1);
        am::texture("meshes/leprechaun_normal.jpg")->bind(2);
        am::texture("meshes/leprechaun_emmisive.png")->bind(3);
#endif
        gl::draw(mMonster);
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
