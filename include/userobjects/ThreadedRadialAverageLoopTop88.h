//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "RadialAverageTop88.h"

#include "libmesh/nanoflann.hpp"

using QPDataRange =
    StoredRange<std::vector<RadialAverageTop88::QPData>::const_iterator, RadialAverageTop88::QPData>;

/**
 * RadialAverage threaded loop
 */
class ThreadedRadialAverageLoopTop88
{
public:
  ThreadedRadialAverageLoopTop88(RadialAverageTop88 &);

  /// Splitting constructor
  ThreadedRadialAverageLoopTop88(const ThreadedRadialAverageLoopTop88 & x, Threads::split split);

  /// dummy virtual destructor
  virtual ~ThreadedRadialAverageLoopTop88() {}

  /// parens operator with the code that is executed in threads
  void operator()(const QPDataRange & range);

  /// thread join method
  virtual void join(const ThreadedRadialAverageLoopTop88 & /*x*/) {}

protected:
  /// rasterizer to manage the sample data
  RadialAverageTop88 & _radavg;

  /// ID number of the current thread
  THREAD_ID _tid;

private:
};
