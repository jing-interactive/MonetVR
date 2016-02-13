#version 100

uniform mat4	ciModelViewProjection;
uniform mat3	ciNormalMatrix;

attribute vec4		ciPosition;
attribute vec2		ciTexCoord0;
//attribute vec3		ciNormal;

//varying lowp vec4	Color;
varying highp vec3	Normal;
varying highp vec2	TexCoord0;

uniform sampler2D 	uTex0;

void main( void )
{
#if 1
    vec3 color = texture2D( uTex0, vec2(ciTexCoord0.t, 1.0 - ciTexCoord0.s) ).rgb;
#else
    vec3 color = texture2D( uTex0, ciTexCoord0 ).rgb;
#endif
    vec4 pos = ciPosition;
    pos.z += (color.r + color.g + color.b) * 0.1;

    gl_Position	= ciModelViewProjection * pos;
    TexCoord0 	= ciTexCoord0;
//    Normal		= ciNormalMatrix * ciNormal;
}
