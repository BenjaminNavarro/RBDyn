/*
 * Copyright 2012-2019 CNRS-UM LIRMM, CNRS-AIST JRL
 */

// associated header
#include "RBDyn/EulerIntegration.h"

// includes
// RBDyn
#include "RBDyn/MultiBody.h"
#include "RBDyn/MultiBodyConfig.h"

namespace
{

/** Compute the sum of the first terms of the Magnus expansion of \Omega such that
 * q' = q*exp(\Omega) is the quaternion obtained after applying a constant acceleration
 * rotation wD for a duration step, while starting with a velocity w
 */
Eigen::Vector3d magnusExpansion(const Eigen::Vector3d & w, const Eigen::Vector3d & wD, double step)
{
  double step2 = step * step;

  Eigen::Vector3d w1 = w + wD * step;
  Eigen::Vector3d O1 = (w + w1) * step / 2;
  Eigen::Vector3d O2 = w1.cross(w) * step2 / 12;
  Eigen::Vector3d O3 = wD.cross(O2) * step2 / 20;

  return O1 + O2 + O3;
}

} // namespace

namespace rbd
{

void eulerJointIntegration(Joint::Type type,
                           const std::vector<double> & alpha,
                           const std::vector<double> & alphaD,
                           double step,
                           std::vector<double> & q)
{
  double step2 = step * step;
  switch(type)
  {
    case Joint::Rev:
    case Joint::Prism:
    {
      q[0] += alpha[0] * step + alphaD[0] * step2 / 2;
      break;
    }

    /// @todo manage reverse joint
    case Joint::Planar:
    {
      // This is the old implementation akin to x' = x + v*step
      // (i.e. we don't take the acceleration into account)
      /// @todo us the acceleration
      double q1Step = q[2] * alpha[0] + alpha[1];
      double q2Step = -q[1] * alpha[0] + alpha[2];
      q[0] += alpha[0] * step;
      q[1] += q1Step * step;
      q[2] += q2Step * step;
      break;
    }

    case Joint::Cylindrical:
    {
      q[0] += alpha[0] * step + alphaD[0] * step2 / 2;
      q[1] += alpha[1] * step + alphaD[1] * step2 / 2;
      break;
    }

    /// @todo manage reverse joint
    case Joint::Free:
    {
      Eigen::Vector3d v;
      Eigen::Vector3d a;
      v << alpha[3], alpha[4], alpha[5];
      a << alphaD[3], alphaD[4], alphaD[5];
      // v and a are in FS coordinate. We have to put it back in FP coordinate.
      v = QuatToE(q).transpose() * v;
      a = QuatToE(q).transpose() * a;

      q[4] += v[0] * step + a[0] * step2 / 2;
      q[5] += v[1] * step + a[1] * step2 / 2;
      q[6] += v[2] * step + a[2] * step2 / 2;

      // don't break, we go in spherical
    }
    /// @todo manage reverse joint
    /* fallthrough */
    case Joint::Spherical:
    {
      Eigen::Quaterniond qi(q[0], q[1], q[2], q[3]);
      Eigen::Vector3d w(alpha[0], alpha[1], alpha[2]);
      Eigen::Vector3d wD(alphaD[0], alphaD[1], alphaD[2]);

      // See https://cwzx.wordpress.com/2013/12/16/numerical-integration-for-rotational-dynamics/
      // the division by 2 is because we want to compute exp(O/2);
      Eigen::Vector3d O = magnusExpansion(w, wD, step) / 2;
      double n = O.norm();
      double s = sva::sinc(n);
      Eigen::Quaterniond qexp(std::cos(n), s * O.x(), s * O.y(), s * O.z());

      qi *= qexp;
      qi.normalize(); // This step should not be necessary but we keep it for robustness

      q[0] = qi.w();
      q[1] = qi.x();
      q[2] = qi.y();
      q[3] = qi.z();
      break;
    }

    case Joint::Fixed:
    default:;
  }
}

void eulerIntegration(const MultiBody & mb, MultiBodyConfig & mbc, double step)
{
  const std::vector<Joint> & joints = mb.joints();

  // integrate
  for(std::size_t i = 0; i < joints.size(); ++i)
  {
    eulerJointIntegration(joints[i].type(), mbc.alpha[i], mbc.alphaD[i], step, mbc.q[i]);
    for(size_t j = 0; j < static_cast<size_t>(joints[i].dof()); ++j)
    {
      mbc.alpha[i][j] += mbc.alphaD[i][j] * step;
    }
  }
}

void sEulerIntegration(const MultiBody & mb, MultiBodyConfig & mbc, double step)
{
  checkMatchQ(mb, mbc);
  checkMatchAlpha(mb, mbc);
  checkMatchAlphaD(mb, mbc);

  eulerIntegration(mb, mbc, step);
}

} // namespace rbd
