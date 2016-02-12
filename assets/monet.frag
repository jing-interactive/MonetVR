#ifdef GL_ES
precision highp float;
#else
#define highp // is this correct?
#endif

uniform sampler2D 		uTex0;

varying highp vec2		TexCoord0;
varying highp vec3		Normal;

void main( void )
{
	vec3 normal = normalize( -Normal );
	float diffuse = max( dot( normal, vec3( 0, 0, -1 ) ), 0.0 );
	vec3 color = texture2D( uTex0, TexCoord0 ).rgb;
	gl_FragColor = vec4( color, 1.0 );
}
