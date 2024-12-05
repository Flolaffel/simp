//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "DensityUpdateMMA.h"
#include "MooseTypes.h"

/**
 * Element user object that performs SIMP optimization using the Method of Moving Asymptotes (MMA)
 * Use after Design Responses and Sensitivity Filter
 */
class DensityUpdateUnconstrainedMMA : public DensityUpdateMMA
{
public:
  static InputParameters validParams();

  DensityUpdateUnconstrainedMMA(const InputParameters & parameters);

protected:
  /**
   * Gathers element data
   */
  // virtual void gatherElementData() override;

  /**
   * Performs the MMA setup
   */
  virtual void performMmaLoop() override;
};
