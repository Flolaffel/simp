//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "Filter.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", Filter);

InputParameters
Filter::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params += Filter::commonParameters();
  params.addClassDescription("Prepares filtering.");
  params.addParam<Real>("radius", "Cut-off radius for the averaging");
  params.addParam<MeshGeneratorName>(
      "mesh_generator",
      "Name of the mesh generator to be used to retrieve control drums information.");
  return params;
}

Filter::Filter(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _filter_type(getParam<MooseEnum>("filter_type").getEnum<FilterType>()),
    _mesh(_subproblem.mesh())
{
  if (_filter_type != FilterType::NONE)
  {
    _mesh_generator = getParam<MeshGeneratorName>("mesh_generator");
    _radius = getParam<Real>("radius");
    _nx = getMeshProperty<unsigned int>("num_elements_x", _mesh_generator);
    _ny = getMeshProperty<unsigned int>("num_elements_y", _mesh_generator);
    _xmin = getMeshProperty<Real>("xmin", _mesh_generator);
    _xmax = getMeshProperty<Real>("xmax", _mesh_generator);
    _ymin = getMeshProperty<Real>("ymin", _mesh_generator);
    _ymax = getMeshProperty<Real>("ymax", _mesh_generator);
  }
}

void
Filter::prepareFilter()
{
  // Only eligibale for elemente size of 1 mm
  int upp_r = ceil(_radius);
  int size = _nx * _ny * std::pow((2 * (upp_r - 1) + 1), 2);
  std::vector<int> iH(size, 1);
  std::vector<int> jH(size, 1);
  std::vector<Real> sH(size, 0);
  int counter = 0;
  for (unsigned int i = 0; i < _ny; i++)
  {
    for (unsigned int j = 0; j < _nx; j++)
    {
      int e1 = i * _nx + j;
      for (int k = std::max<int>(i - (upp_r - 1), 0); k < std::min<int>(i + upp_r, _ny); k++)
      {
        for (int l = std::max<int>(j - (upp_r - 1), 0); l < std::min<int>(j + upp_r, _nx); l++)
        {
          int e2 = k * _nx + l;
          iH[counter] = e1;
          jH[counter] = e2;
          sH[counter] = std::max<double>(0,
                                         _radius - std::sqrt(std::pow(std::abs<int>(i - k), 2) +
                                                             std::pow(std::abs<int>(j - l), 2)));
          counter++;
        }
      }
    }
  }

  // Fill _H and with values sH at locations iH,jH
  _H.resize(_nx * _ny, std::vector<Real>(_nx * _ny, 0));
  _Hs.resize(_nx * _ny);
  for (int i = 0; i < counter; i++)
  {
    _H[iH[i]][jH[i]] = sH[i];
  }

  // Fill _Hs with the column sums of _H
  for (unsigned int i = 0; i < _H.size(); i++)
  {
    Real column_sum = 0;
    for (unsigned int j = 0; j < _H[i].size(); j++)
    {
      column_sum += _H[i][j];
    }
    _Hs[i] = column_sum;
  }
}

MooseEnum
Filter::getFilterEnum()
{
  auto filter = MooseEnum("none sensitivity density", "none");

  filter.addDocumentation("none", "No filter.");
  filter.addDocumentation("sensitivity", "Sensitivity filter.");
  filter.addDocumentation("density", "Density filter.");
  return filter;
}

InputParameters
Filter::commonParameters()
{
  InputParameters params = emptyInputParameters();

  params.addParam<MooseEnum>("filter_type", Filter::getFilterEnum(), "The filter type");

  return params;
}
