// https://github.com/OpenHMD/OpenHMD/blob/master/examples/opengl/shaders/test1.vert.glsl
uniform sampler2D u_texture;
uniform int u_scr_w = 640;
uniform int u_scr_h = 800;

const vec2 LeftLensCenter = vec2(0.2863248, 0.5);
const vec2 RightLensCenter = vec2(0.7136753, 0.5);
const vec2 LeftScreenCenter = vec2(0.25, 0.5);
const vec2 RightScreenCenter = vec2(0.75, 0.5);
const vec2 Scale = vec2(0.1469278, 0.2350845);
const vec2 ScaleIn = vec2(4, 2.5);
const vec4 HmdWarpParam   = vec4(1, 0.22, 0.24, 0);

// Scales input texture coordinates for distortion.
vec2 HmdWarp(vec2 in01, vec2 LensCenter)
{
  vec2 theta = (in01 - LensCenter) * ScaleIn; // Scales to [-1, 1]
  float rSq = theta.x * theta.x + theta.y * theta.y;
  vec2 rvector = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq +
    HmdWarpParam.z * rSq * rSq +
    HmdWarpParam.w * rSq * rSq * rSq);
  return LensCenter + Scale * rvector;
}

void main()
{
  if (abs(gl_FragCoord.x - u_scr_w) < 2) {
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    return;
  }

  vec2 LensCenter = gl_FragCoord.x < u_scr_w ? LeftLensCenter : RightLensCenter;
  vec2 ScreenCenter = gl_FragCoord.x < u_scr_w ? LeftScreenCenter : RightScreenCenter;

  vec2 oTexCoord = gl_FragCoord.xy / vec2(u_scr_w * 2, u_scr_h);

  vec2 tc = HmdWarp(oTexCoord, LensCenter);
  if (any(bvec2(clamp(tc,ScreenCenter-vec2(0.25,0.5), ScreenCenter+vec2(0.25,0.5)) - tc)))
  {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  tc.x = gl_FragCoord.x < 640 ? (2.0 * tc.x) : (2.0 * (tc.x - 0.5));
  gl_FragColor = texture2D(u_texture, tc);
}