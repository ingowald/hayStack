// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "Camera.h"
#include "GLFWBaseViewer.h"

namespace hs {
  namespace viewer {

    vec3f Camera::getFrom() const
    {
      return position;
    }
    
    vec3f Camera::getAt() const
    {
      return position - frame.vz;
    }

    vec3f Camera::getUp() const
    {
      return frame.vy;
    }
      
    void Camera::setFovy(const float fovy)
    {
      this->fovyInDegrees = fovy;
    }

    void Camera::setAspect(const float aspect)
    {
      this->aspect = aspect;
    }

    void Camera::setFocalDistance(float focalDistance)
    {
      this->focalDistance = focalDistance;
    }

    /*! tilt the frame around the z axis such that the y axis is
        "facing upwards" */
    void Camera::forceUpFrame()
    {
      // frame.vz remains unchanged
      if (fabsf(dot(frame.vz,upVector)) < 1e-6f)
        // looking along upvector; not much we can do here ...
        return;
      frame.vx = normalize(cross(upVector,frame.vz));
      frame.vy = normalize(cross(frame.vz,frame.vx));
    }

    void Camera::setOrientation(/* camera origin    : */const vec3f &origin,
                                    /* point of interest: */const vec3f &interest,
                                    /* up-vector        : */const vec3f &up,
                                    /* fovy, in degrees : */float fovyInDegrees,
                                    /* set focal dist?  : */bool  setFocalDistance)
    {
      this->fovyInDegrees = fovyInDegrees;
      position = origin;
      upVector = up;
      frame.vz
        = (interest==origin)
        ? vec3f(0,0,1)
        : /* negative because we use NEGATIZE z axis */ - normalize(interest - origin);
      frame.vx = cross(up,frame.vz);
      if (dot(frame.vx,frame.vx) < 1e-8f)
        frame.vx = vec3f(0,1,0);
      else
        frame.vx = normalize(frame.vx);
      // frame.vx
      //   = (fabs(dot(up,frame.vz)) < 1e-6f)
      //   ? vec3f(0,1,0)
      //   : normalize(cross(up,frame.vz));
      frame.vy = normalize(cross(frame.vz,frame.vx));
      poiDistance = length(interest-origin);
      if (setFocalDistance) focalDistance = poiDistance;
      forceUpFrame();
    }


    /*! this gets called when the user presses a key on the keyboard ... */
    void CameraManipulator::key(char key, const vec2i &where)
    {
      Camera &fc = viewer->camera;

      switch(key) {
      case 'f':
      case 'F':
        if (viewer->flyModeManipulator)
          viewer->cameraManipulator = viewer->flyModeManipulator;
        break;
      case 'i':
      case 'I':
        if (viewer->inspectModeManipulator)
          viewer->cameraManipulator = viewer->inspectModeManipulator;
        break;
      case '+':
      case '=':
        fc.motionSpeed *= 2.f;
        std::cout << "# viewer: new motion speed is " << fc.motionSpeed << std::endl;
        break;
      case '-':
      case '_':
        fc.motionSpeed /= 2.f;
        std::cout << "# viewer: new motion speed is " << fc.motionSpeed << std::endl;
        break;
      case 'C': {
        // for anybody deriving from this class: if you do not like
        // this output, simply intercept the 'C' key in your derived
        // 'key()' method before calling this...
        std::cout << "(C)urrent camera:" << std::endl;
        std::cout << "- from :" << fc.position << std::endl;
        std::cout << "- poi  :" << fc.getPOI() << std::endl;
        std::cout << "- upVec:" << fc.upVector << std::endl;
        std::cout << "- frame:" << fc.frame << std::endl;

        const vec3f vp = fc.position;
        const vec3f vi = fc.getPOI();
        const vec3f vu = fc.upVector;
        const float fovy = fc.getFovyInDegrees();
        std::cout << "(suggested cmdline format, for apps that support this:) "
                  << " --camera"
                  << " " << vp.x << " " << vp.y << " " << vp.z
                  << " " << vi.x << " " << vi.y << " " << vi.z
                  << " " << vu.x << " " << vu.y << " " << vu.z
                  << " -fovy " << fovy
                  << std::endl;
      } break;
      case 'x':
      case 'X':
        fc.setUpVector(fc.upVector==vec3f(1,0,0)?vec3f(-1,0,0):vec3f(1,0,0));
        viewer->updateCamera();
        break;
      case 'y':
      case 'Y':
        fc.setUpVector(fc.upVector==vec3f(0,1,0)?vec3f(0,-1,0):vec3f(0,1,0));
        viewer->updateCamera();
        break;
      case 'z':
      case 'Z':
        fc.setUpVector(fc.upVector==vec3f(0,0,1)?vec3f(0,0,-1):vec3f(0,0,1));
        viewer->updateCamera();
        break;
      default:
        break;
      }
    }

  } // ::hs::viewer
} // ::hs

