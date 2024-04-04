//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "StressResponseBase.h"

/**
 * Element user object that copmutes the sensitivity need for a stress constrained Solid Isotropic
 * Material Penalization (SIMP) optimization.
 */
class StressResponseVerbartPMean : public StressResponseBase
{
public:
  static InputParameters validParams();

  StressResponseVerbartPMean(const InputParameters & parameters);

  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

  virtual void computeStress() override;
  virtual void computeValue() override;
  virtual void computeSensitivity() override;

protected:
  /// Voigt stress vector for every Element
  RealEigenMatrix _stress;
  /// Elemental Von Mises stress vector
  RealEigenVector _vonmises;
};
