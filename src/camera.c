#include <splatc/camera.h>

mat4
camera_get_view(camera *camera) {
  mat4 view = mat4_id();

  vec3f forward = norm3(sub3(camera->at, camera->pos));
  vec3f right = norm3(cross3(forward, camera->up));
  vec3f up = norm3(cross3(right, forward));

  camera->forward = (vec3f){-forward.x, -forward.y, -forward.z};
  camera->right = right;
  camera->up = up;

  view.vv[0][0] = right.v[0];
  view.vv[0][1] = right.v[1];
  view.vv[0][2] = right.v[2];

  view.vv[1][0] = up.v[0];
  view.vv[1][1] = up.v[1];
  view.vv[1][2] = up.v[2];

  view.vv[2][0] = forward.v[0];
  view.vv[2][1] = forward.v[1];
  view.vv[2][2] = forward.v[2];

  view.vv[3][0] = -dot3(right, camera->pos);
  view.vv[3][1] = -dot3(up, camera->pos);
  view.vv[3][2] = -dot3(forward, camera->pos);

  view.vv[0][3] = view.vv[1][3] = view.vv[2][3] = 0.0f;
  view.vv[3][3] = 1.0f;

  return view;
}

mat4
camera_get_projection(camera *camera) {
  float t = 1.f / tan(camera->fovy * 0.5f);

  mat4 proj = mat4_id();

  proj.vv[0][0] = t / camera->aspect;
  proj.vv[1][1] = t;
  proj.vv[2][2] = (camera->far + camera->near) / (camera->near - camera->far);
  proj.vv[2][3] = -1.0f;
  proj.vv[3][2] =
      -(2.0f * camera->far * camera->near) / (camera->near - camera->far);
  proj.vv[3][3] = 0.0f;
  return proj;
}
