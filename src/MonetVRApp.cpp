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
#include "DistortionHelper.h"

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
    
    bool mIsStereo = true;
    bool mIsMonsterVisible = false;
    
    void setup() override;
    void update() override;
    
    void resize() override
    {
        sizeInPixel.x = toPixels(getWindowWidth()); // shorter edge
        sizeInPixel.y = toPixels(getWindowHeight());
        
        gl::Fbo::Format format;
        format.enableDepthBuffer();
//        format.setSamples( 2 );
        mFbo = gl::Fbo::create( sizeInPixel.x, sizeInPixel.y, format );
    }
    
    void cleanup() override
    {
        if (mLoadingThread)
            mLoadingThread->join();
    }
    
    void draw() override;
    
    void render(const CameraPersp& camera);
    void setupEyeInfos();
    
    CaptureHelper mCaptureHelper;
    MotionHelper mMotionHelper;
    DistortionHelperRef mDistortionHelper;

    //
    // gl
    //
    gl::VboMeshRef mMonster;
    gl::FboRef mFbo;
    
    void setupLoadingThread();
    shared_ptr<thread>		mLoadingThread;
    
//    gl::GlslProgRef     mGlsl;
    
    gl::BatchRef           mBatch;
    
    vec2 sizeInPixel;
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
    TriMesh triMesh;
    
    int mVboWidth = mCaptureHelper.size.x;
    int mVboHeight = mCaptureHelper.size.y;
    
    int dx = 2;
    
    for( int x = 0; x < mVboWidth; ++x ) {
        for( int z = 0; z < mVboHeight; ++z ) {
            // create a quad for each vertex, except for along the bottom and right edges
            if (x % dx == 0 && z % dx == 0) {
                if( ( x + dx < mVboWidth ) && ( z + dx < mVboHeight ) ) {
                    uint32_t v00 = (x+0) * mVboHeight + (z+0);
                    uint32_t v10 = (x+dx) * mVboHeight + (z+0);
                    uint32_t v11 = (x+dx) * mVboHeight + (z+dx);
                    uint32_t v01 = (x+0) * mVboHeight + (z+dx);
                    triMesh.appendTriangle(v00, v01, v10);
                    triMesh.appendTriangle(v10, v01, v11);
                }
            }
            // the texture coordinates are mapped to [0,1.0)
            triMesh.appendTexCoord( { x / (float)mVboWidth, 1 - (z / (float)mVboHeight) } );
            float xp = ( x - mVboWidth * 0.5f );
            float zp = ( z - mVboHeight * 0.5f );
            float scale = 0.002f;
            triMesh.appendPosition( { xp * scale, zp * scale, 0.0f } );
        }
    }

    //        auto glsl = gl::getStockShader( gl::ShaderDef().texture());
    // triMesh.recalculateNormals();
    auto glsl = am::glslProg("shaders/monet.vert", "shaders/monet.frag");
    mBatch = gl::Batch::create(triMesh, glsl);
}

void MonetVRApp::setup()
{
    CaptureHelper::printDevices();
    
    mCaptureHelper.setup();
    mMotionHelper.setup();
    
    mDistortionHelper = DistortionHelper::create( false );
    
    getWindow()->getSignalMouseDown().connect( [this]( MouseEvent event ) {
        mIsStereo = !mIsStereo;
    });

    setupEyeInfos();
    
    setupLoadingThread();
    
    gl::disableBlending();
}

void MonetVRApp::update()
{

}

void MonetVRApp::draw()
{
    gl::clear();

    CameraStereo camera;
    camera.setEyeSeparation( 0.015 );
//    camera.setConvergence(0);
//    camera.setFov( 125.871f );
    camera.lookAt( vec3( 0, 0, 10 ), vec3( 0 ) );
    
    float w = sizeInPixel.x;
    float h = sizeInPixel.y;
    
    if (mIsStereo)
    {
        // 3d
        {
            gl::ScopedFramebuffer scopedFbo(mFbo);
            gl::clear();
            
            camera.setPerspective( 60, w / 2 / h, 1, 1000 );
            
            // left
            gl::ScopedViewport viewport(0,0,w/2,h);
            camera.enableStereoLeft();
            render(camera);
            
            // right
            gl::viewport(w/2,0,w/2,h);
            camera.enableStereoRight();
            render(camera);
        }
        
        // Distortion Post-processing
        gl::setMatricesWindow( getWindowSize(), false );
        gl::disableDepthRead();
        gl::disableDepthWrite();
        
        mDistortionHelper->render( mFbo->getColorTexture(), getWindowBounds());
    }
    else
    {
        camera.setPerspective( 60, w / h, 1, 1000 );

        camera.disableStereo();
        render(camera);
    }
}

void MonetVRApp::render(const CameraPersp& camera)
{
    gl::enableDepthRead();
//    gl::enableDepthWrite();
//    gl::setMatricesWindow(getWindowSize());
    
    gl::setMatrices( camera );

    if ( mCaptureHelper.isReady()) {
        gl::ScopedModelMatrix modelScope;
//        gl::ScopedGlslProg glslProg( gl::getStockShader( gl::ShaderDef().texture().color() ));
        
#if 1
        mCaptureHelper.texture->bind();
#else
        am::texture("checkerboard.jpg")->bind(0);
#endif
//        mGlsl->uniform( "uTex0", 0 );
        
        gl::scale(vec3(7, 7, 1.5));
        mBatch->draw();
    }

//    gl::multViewMatrix( mMotionHelper.deviceRotation );
    
    gl::disableDepthRead();
    gl::drawCoordinateFrame();
    gl::enableDepthRead();
//    gl::drawColorCube( vec3(), vec3( 3 ) );
    
    if (mMonster && mIsMonsterVisible)
    {
        gl::ScopedModelMatrix modelScope;
        gl::multModelMatrix(mMotionHelper.deviceRotation);
        gl::translate(0, 0, 1);
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
