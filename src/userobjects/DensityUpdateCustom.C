//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateCustom.h"
#include <algorithm>

registerMooseObject("OptimizationApp", DensityUpdateCustom);

InputParameters
DensityUpdateCustom::validParams()
{
  InputParameters params = Filter::validParams();
  params.addClassDescription(
      "Compute updated densities based on sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified.");
  params.addParam<MooseEnum>(
      "update_scheme", DensityUpdateCustom::getUpdateSchemeEnum(), "The update scheme");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addCoupledVar("old_design_density1", "Design density one iteration ago variable name.");
  params.addCoupledVar("old_design_density2", "Design density two iterations ago variable name.");
  params.addRequiredParam<VariableName>("compliance_sensitivity",
                                        "Name of the compliance sensitivity variable.");
  params.addRequiredParam<VariableName>("volume_sensitivity",
                                        "Name of the volume sensitivity variable.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addCoupledVar("mma_lower_asymptotes",
                       "Column vector with the lower asymptotes from the previous "
                       "iteration (provided that iter>1).");
  params.addCoupledVar("mma_upper_asymptotes",
                       "Column vector with the upper asymptotes from the previous "
                       "iteration (provided that iter>1).");
  params.addParam<Real>("bisection_lower_bound", 0, "Lower bound for the bisection algorithm.");
  params.addParam<Real>("bisection_upper_bound", 1e9, "Upper bound for the bisection algorithm.");
  params.addParam<int>(
      "execution_order_group",
      1,
      "Execution order groups are executed in increasing order (e.g., the lowest "
      "number is executed first). Note that negative group numbers may be used to execute groups "
      "before the default (0) group. Please refer to the user object documentation "
      "for ordering of user object execution within a group.");
  params.addCoupledVar("physical_density", "Physical density variable name.");
  return params;
}

DensityUpdateCustom::DensityUpdateCustom(const InputParameters & parameters)
  : Filter(parameters),
    _update_scheme(getParam<MooseEnum>("update_scheme").getEnum<UpdateScheme>()),
    _design_density(&writableVariable("design_density")),
    _physical_density(&writableVariable("physical_density")),
    _compliance_sensitivity_name(getParam<VariableName>("compliance_sensitivity")),
    _compliance_sensitivity(&_subproblem.getStandardVariable(_tid, _compliance_sensitivity_name)),
    _volume_sensitivity_name(getParam<VariableName>("volume_sensitivity")),
    _volume_sensitivity(&_subproblem.getStandardVariable(_tid, _volume_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
  if (!dynamic_cast<MooseVariableFE<Real> *>(_physical_density))
    paramError("physical_density", "Physical density must be a finite element variable");
  if (_filter_type == FilterType::SENSITIVITY)
    paramError("filter_type", "Sensitivity filtering is not a viable option for density update");

  if (_update_scheme == UpdateScheme::OC)
  {
    _lower_bound = getParam<Real>("bisection_lower_bound");
    _upper_bound = getParam<Real>("bisection_upper_bound");
  }
  else if (_update_scheme == UpdateScheme::MMA)
  {
    _old_design_density1 = &writableVariable("old_design_density1");
    _old_design_density2 = &writableVariable("old_design_density2");
    _lower_asymptotes = &writableVariable("mma_lower_asymptotes");
    _upper_asymptotes = &writableVariable("mma_upper_asymptotes");
  }
}

void
DensityUpdateCustom::initialize()
{
  gatherElementData();
  if (_filter_type == FilterType::DENSITY)
    Filter::prepareFilter();
  if (_update_scheme == UpdateScheme::OC)
    performOptimCritLoop();
  else if (_update_scheme == UpdateScheme::MMA)
    performMmaLoop();
}

void
DensityUpdateCustom::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)
        ->setNodalValue(elem_data.new_design_density);
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.new_phys_density);
    if (_update_scheme == UpdateScheme::MMA)
    {
      dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)
          ->setNodalValue(elem_data.current_design_density);
      dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)
          ->setNodalValue(elem_data.old_design_density1);
      dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->setNodalValue(elem_data.new_lower);
      dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->setNodalValue(elem_data.new_upper);
    }
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateCustom::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data;
      if (_update_scheme == UpdateScheme::OC)
      {
        data = ElementData(
            dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
            0,
            0,
            0,
            dynamic_cast<const MooseVariableFE<Real> *>(_compliance_sensitivity)
                ->getElementalValue(elem),
            dynamic_cast<const MooseVariableFE<Real> *>(_volume_sensitivity)
                ->getElementalValue(elem),
            0,
            0,
            elem->volume(),
            0,
            0,
            0,
            0);
      }
      else if (_update_scheme == UpdateScheme::MMA)
      {
        data = ElementData(
            dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
            dynamic_cast<MooseVariableFE<Real> *>(_physical_density)->getElementalValue(elem),
            dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)->getElementalValue(elem),
            dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)->getElementalValue(elem),
            dynamic_cast<const MooseVariableFE<Real> *>(_compliance_sensitivity)
                ->getElementalValue(elem),
            dynamic_cast<const MooseVariableFE<Real> *>(_volume_sensitivity)
                ->getElementalValue(elem),
            dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->getElementalValue(elem),
            dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->getElementalValue(elem),
            elem->volume(),
            0,
            0,
            0,
            0);
      }
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateCustom::performOptimCritLoop()
{
  // Initialize the lower and upper bounds for the bisection method
  Real l1 = _lower_bound;
  Real l2 = _upper_bound;
  bool perform_loop = true;
  // Loop until the relative difference between l1 and l2 is less than a small tolerance
  while (perform_loop)
  {
    // Compute the midpoint between l1 and l2
    Real lmid = 0.5 * (l2 + l1);

    // Initialize a vector holding the new densities
    unsigned int n_el = _elem_data_map.size();
    std::vector<Real> new_density(n_el);
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      new_density[id] = computeUpdatedDensity(elem_data.current_design_density,
                                              elem_data.compliance_sensitivity,
                                              elem_data.volume_sensitivity,
                                              lmid);
    }
    std::vector<Real> filt_density = new_density;

    if (_filter_type == FilterType::DENSITY)
      filt_density = DensityFilter(new_density);

    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Assign new values
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Update the current filtered density for the current element
      elem_data.new_design_density = new_density[id];
      elem_data.new_phys_density = filt_density[id];
      curr_total_volume += filt_density[id] * elem_data.volume;
    }

    // Sum the current total volume across all processors
    _communicator.sum(curr_total_volume);

    // Update l1 or l2 based on whether the current total volume is greater than the total
    // allowable volume
    if (curr_total_volume > _total_allowable_volume)
      l1 = lmid;
    else
      l2 = lmid;

    // Determine whether to continue the loop based on the relative difference between l1 and l2
    perform_loop = (l2 - l1) / (l1 + l2) > 1e-3;
  }
}

// Method to compute the updated density for an element
Real
DensityUpdateCustom::computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid)
{
  // Define the maximum allowable change in density
  Real move = 0.2;
  // Compute the updated density based on the current density, the sensitivity, and the midpoint
  // value
  Real updated_density =
      std::max(0.0,
               std::max(current_density - move,
                        std::min(1.0,
                                 std::min(current_density + move,
                                          current_density * std::sqrt(-dc / dv / lmid)))));
  // Return the updated density
  return updated_density;
}

void
DensityUpdateCustom::performMmaLoop()
{
  int m = 1;
  int n = _elem_data_map.size();

  // Vector variables of size n
  std::vector<Real> xmin(n, 0), xmax(n, 1), xold1(n, 0), xold2(n, 0), low(n, 0), upp(n, 0),
      xval(n, 0), df0dx(n, 0), dfdx(n, 0);

  // Scalar constants for the case of one constraint (m=1)
  Real a0 = 1;
  Real a = 0;
  Real c_MMA = 10000;
  Real d = 0;
  Real fval = 0;

  // Loop over all elements to populate the vectors
  for (auto && [id, elem_data] : _elem_data_map)
  {
    xval[id] = elem_data.current_design_density;
    xold1[id] = elem_data.old_design_density1;
    xold2[id] = elem_data.old_design_density2;
    df0dx[id] = elem_data.compliance_sensitivity;
    dfdx[id] = elem_data.volume_sensitivity / (_volume_fraction * n);
    low[id] = elem_data.lower;
    upp[id] = elem_data.upper;
    fval += elem_data.current_physical_density;
  }
  fval /= _volume_fraction * n;
  fval -= 1;

  /// MMA

  // die hier spaeter als Parameter übergeben
  Real epsimin = 0.0000001;
  Real raa0 = 0.00001;
  Real move = 0.5;
  Real albefa = 0.1;
  Real asyinit = 0.5;
  Real asyincr = 1.2;
  Real asydecr = 0.7;
  std::vector<Real> eeen(n, 1);
  std::vector<Real> eeem(m, 1);

  // Calculation of the asymptotes low and upp
  std::vector<Real> zzz(n, 0);
  if (_t_step <= 2)
  {
    for (int i = 0; i < n; i++)
    {
      low[i] = xval[i] - asyinit * (xmax[i] - xmin[i]);
      upp[i] = xval[i] + asyinit * (xmax[i] - xmin[i]);
    }
  }
  else
  {
    for (int i = 0; i < n; i++)
    {
      zzz[i] = (xval[i] - xold1[i]) * (xold1[i] - xold2[i]);
      Real factor;
      if (zzz[i] > 0)
        factor = asyincr;
      else
        factor = asydecr;
      low[i] = xval[i] - factor * (xold1[i] - low[i]);
      upp[i] = xval[i] + factor * (upp[i] - xold1[i]);
      Real lowmin, lowmax, uppmin, uppmax;
      lowmin = xval[i] - 10 * (xmax[i] - xmin[i]);
      lowmax = xval[i] - 0.01 * (xmax[i] - xmin[i]);
      uppmin = xval[i] + 0.01 * (xmax[i] - xmin[i]);
      uppmax = xval[i] + 10 * (xmax[i] - xmin[i]);
      low[i] = std::max(low[i], lowmin);
      low[i] = std::min(low[i], lowmax);
      upp[i] = std::min(upp[i], uppmax);
      upp[i] = std::max(upp[i], uppmin);
    }
  }

  // Calculation of the bounds alpha and beta
  std::vector<Real> alpha(n, 0), beta(n, 0);
  for (int i = 0; i < n; i++)
  {
    Real zzz1, zzz2;
    zzz1 = low[i] + albefa * (xval[i] - low[i]);
    zzz2 = xval[i] - move * (xmax[i] - xmin[i]);
    zzz[i] = std::max(zzz1, zzz2);
    alpha[i] = std::max(zzz[i], xmin[i]);
    zzz1 = upp[i] - albefa * (upp[i] - xval[i]);
    zzz2 = xval[i] + move * (xmax[i] - xmin[i]);
    zzz[i] = std::min(zzz1, zzz2);
    beta[i] = std::min(zzz[i], xmax[i]);
  }

  // Calculations of p0, q0, P, Q and b
  std::vector<Real> p0(n, 0), q0(n, 0), P(n, 0), Q(n, 0);
  Real b = 0;
  Real xmami, xmamieps, xmamiinv, ux1, ux2, xl1, xl2, uxinv, xlinv, pq0, PQ;
  for (int i = 0; i < n; i++)
  {
    xmami = xmax[i] - xmin[i];
    xmamieps = 0.00001 * eeen[i];
    xmami = std::max(xmami, xmamieps);
    xmamiinv = eeen[i] / xmami;
    ux1 = upp[i] - xval[i];
    ux2 = ux1 * ux1;
    xl1 = xval[i] - low[i];
    xl2 = xl1 * xl1;
    uxinv = eeen[i] / ux1;
    xlinv = eeen[i] / xl1;

    p0[i] = std::max(df0dx[i], 0.0);
    q0[i] = std::max(-df0dx[i], 0.0);
    pq0 = 0.001 * (p0[i] + q0[i]) + raa0 * xmamiinv;
    p0[i] += pq0;
    q0[i] += pq0;
    p0[i] *= ux2;
    q0[i] *= xl2;

    P[i] = std::max(dfdx[i], 0.0);
    Q[i] = std::max(-dfdx[i], 0.0);
    PQ = 0.001 * (P[i] + Q[i]) + raa0 * eeem[0] * xmamiinv;
    P[i] += PQ;
    Q[i] += PQ;
    P[i] *= ux2;
    Q[i] *= xl2;
    b += P[i] * uxinv + Q[i] * xlinv;
  }
  b -= fval;

  // Solving the subproblem by a primal-dual Newton method
  std::vector<Real> new_density =
      MmaSubSolve(m, n, epsimin, low, upp, alpha, beta, p0, q0, P, Q, a0, a, b, c_MMA, d);

  std::vector<Real> filt_density = new_density;

  if (_filter_type == FilterType::DENSITY)
    filt_density = DensityFilter(new_density);

  for (auto && [id, elem_data] : _elem_data_map)
  {
    // Update the current filtered density for the current element
    elem_data.new_design_density = new_density[id];
    elem_data.new_phys_density = filt_density[id];
    elem_data.new_lower = low[id];
    elem_data.new_upper = upp[id];
  }
}

// Method to solve the MMA subproblem by a primal-dual Newton method
std::vector<Real>
DensityUpdateCustom::MmaSubSolve(Real m,
                                 Real n,
                                 Real epsimin,
                                 std::vector<Real> low,
                                 std::vector<Real> upp,
                                 std::vector<Real> alpha,
                                 std::vector<Real> beta,
                                 std::vector<Real> p0,
                                 std::vector<Real> q0,
                                 std::vector<Real> P,
                                 std::vector<Real> Q,
                                 Real a0,
                                 Real a,
                                 Real b,
                                 Real c,
                                 Real d)
{
  std::vector<Real> een(n, 1);
  Real eem = 1.0;
  Real epsi = 1.0;
  Real epsvecm = epsi * eem;
  Real y = eem;
  Real z = 1.0;
  Real lam = eem;
  Real mu = std::max(eem, 0.5 * c);
  Real zet = 1;
  Real s = eem;
  std::vector<Real> epsvecn(n, 0), x(n, 0), xsi(n, 0), eta(n, 0);
  for (int i = 0; i < n; i++)
  {
    epsvecn[i] = epsi * een[i];
    x[i] = 0.5 * (alpha[i] + beta[i]);
    xsi[i] = een[i] / (x[i] - alpha[i]);
    xsi[i] = std::max(xsi[i], een[i]);
    eta[i] = een[i] / (beta[i] - x[i]);
    eta[i] = std::max(eta[i], een[i]);
  }

  while (epsi > epsimin)
  {
    epsvecm = epsi * eem;
    std::vector<Real> ux1(n, 0), xl1(n, 0), ux2(n, 0), xl2(n, 0), uxinv1(n, 0), xlinv1(n, 0),
        plam(n, 0), qlam(n, 0), dpsidx(n, 0), rex(n, 0), rexsi(n, 0), reeta(n, 0);
    Real gvec = 0;
    for (int i = 0; i < n; i++)
    {
      epsvecn[i] = epsi * een[i];
      ux1[i] = upp[i] - x[i];
      xl1[i] = x[i] - low[i];
      ux2[i] = ux1[i] * ux1[i];
      xl2[i] = xl1[i] * xl1[i];
      uxinv1[i] = een[i] / ux1[i];
      xlinv1[i] = een[i] / xl1[i];
      plam[i] = p0[i] + P[i] * lam;
      qlam[i] = q0[i] + Q[i] * lam;
      gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
      dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
      rex[i] = dpsidx[i] - xsi[i] + eta[i];
      rexsi[i] = xsi[i] * (x[i] - alpha[i]) - epsvecn[i];
      reeta[i] = eta[i] * (beta[i] - x[i]) - epsvecn[i];
    }
    Real rey = c + d * y - mu - lam;
    Real rez = a0 - zet - a * lam;
    Real relam = gvec - a * z - y + s - b;
    Real remu = mu * y - epsvecm;
    Real rezet = zet * z - epsi;
    Real res = lam * s - epsvecm;

    std::vector<Real> residu1;
    residu1.reserve(rex.size() + 2);
    residu1.assign(std::begin(rex), std::end(rex));
    residu1.push_back(rey);
    residu1.push_back(rez);

    std::vector<Real> residu2{relam};
    residu2.reserve(residu2.size() + rexsi.size() + reeta.size() + 3);
    residu2.insert(std::end(residu2), std::begin(rexsi), std::end(rexsi));
    residu2.insert(std::end(residu2), std::begin(reeta), std::end(reeta));
    residu2.insert(std::end(residu2), {remu, rezet, res});

    std::vector<Real> residu;
    residu.reserve(residu1.size() + residu2.size());
    residu.assign(std::begin(residu1), std::end(residu1));
    residu.insert(std::end(residu), std::begin(residu2), std::end(residu2));

    Real residunorm = NormVec(residu);
    std::vector<Real> residuabs = AbsVec(residu);
    Real residumax = *std::max_element(std::begin(residuabs), std::end(residuabs));
    int ittt = 0;
    while (residumax > 0.9 * epsi && ittt < 200)
    {
      ittt++;
      std::vector<Real> ux3(n, 0), xl3(n, 0), uxinv2(n, 0), xlinv2(n, 0), GG(n, 0), delx(n, 0),
          diagx(n, 0), diagxinv(n, 0);
      gvec = 0.0;
      for (int i = 0; i < n; i++)
      {
        ux1[i] = upp[i] - x[i];
        xl1[i] = x[i] - low[i];
        ux2[i] = ux1[i] * ux1[i];
        xl2[i] = xl1[i] * xl1[i];
        ux3[i] = ux1[i] * ux2[i];
        xl3[i] = xl1[i] * xl2[i];
        uxinv1[i] = een[i] / ux1[i];
        xlinv1[i] = een[i] / xl1[i];
        uxinv2[i] = een[i] / ux2[i];
        xlinv2[i] = een[i] / xl2[i];
        plam[i] = p0[i] + P[i] * lam;
        qlam[i] = q0[i] + Q[i] * lam;
        gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
        GG[i] = P[i] * uxinv2[i] - Q[i] * xlinv2[i];
        dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
        delx[i] = dpsidx[i] - epsvecn[i] / (x[i] - alpha[i]) + epsvecn[i] / (beta[i] - x[i]);
        diagx[i] = plam[i] / ux3[i] + qlam[i] / xl3[i];
        diagx[i] = 2 * diagx[i] + xsi[i] / (x[i] - alpha[i]) + eta[i] / (beta[i] - x[i]);
        diagxinv[i] = een[i] / diagx[i];
      }
      Real dely = c + d * y - lam - epsvecm / y;
      Real delz = a0 - a * lam - epsi / z;
      Real dellam = gvec - a * z - y - b + epsvecm / lam;
      Real diagy = d + mu / y;
      Real diagyinv = eem / diagy;
      Real diaglam = s / lam;
      Real diaglamyi = diaglam + diagyinv;

      std::vector<Real> dx(n, 0);
      Real dlam, dz;
      if (m < n)
      {
        Real blam = dellam + dely / diagy;
        Real Alam = diaglamyi;
        for (int i = 0; i < n; i++)
        {
          blam -= GG[i] * (delx[i] / diagx[i]);
          Alam += GG[i] * diagxinv[i] * GG[i];
        }
        std::vector<Real> bb{blam, delz};
        std::vector<Real> AA{Alam, a, a, -zet / z};
        Real factor = 1 / ((AA[0] * AA[3]) - (AA[1] * AA[2]));
        std::vector<Real> AAinv{factor * AA[3], factor * -AA[1], factor * -AA[2], factor * AA[0]};
        std::vector<Real> solut{AAinv[0] * bb[0] + AAinv[1] * bb[1],
                                AAinv[2] * bb[0] + AAinv[3] * bb[1]};
        dlam = solut[0];
        dz = solut[1];
        for (int i = 0; i < n; i++)
        {
          dx[i] = -delx[i] / diagx[i] - (GG[i] * dlam) / diagx[i];
        }
      }
      else
      {
        // idk
      }

      std::vector<Real> dxsi(n, 0), deta(n, 0);
      for (int i = 0; i < n; i++)
      {
        dxsi[i] = -xsi[i] + epsvecn[i] / (x[i] - alpha[i]) - (xsi[i] * dx[i]) / (x[i] - alpha[i]);
        deta[i] = -eta[i] + epsvecn[i] / (beta[i] - x[i]) + (eta[i] * dx[i]) / (beta[i] - x[i]);
      }
      Real dy = -dely / diagy + dlam / diagy;
      Real dmu = -mu + epsvecm / y - (mu * dy) / y;
      Real dzet = -zet + epsi / z - zet * dz / z;
      Real ds = -s + epsvecm / lam - (s * dlam) / lam;

      std::vector<Real> xx{y, z, lam};
      xx.reserve(xx.size() + xsi.size() + eta.size() + 3);
      xx.insert(std::end(xx), std::begin(xsi), std::end(xsi));
      xx.insert(std::end(xx), std::begin(eta), std::end(eta));
      xx.insert(std::end(xx), {mu, zet, s});

      std::vector<Real> dxx{dy, dz, dlam};
      dxx.reserve(dxx.size() + dxsi.size() + deta.size() + 3);
      dxx.insert(std::end(dxx), std::begin(dxsi), std::end(dxsi));
      dxx.insert(std::end(dxx), std::begin(deta), std::end(deta));
      dxx.insert(std::end(dxx), {dmu, dzet, ds});

      std::vector<Real> stepxx(xx.size());
      for (unsigned int i = 0; i < xx.size(); i++)
      {
        stepxx[i] = -1.01 * dxx[i] / xx[i];
      }
      std::vector<Real> stepalpha(n), stepbeta(n);
      for (unsigned int i = 0; i < n; i++)
      {
        stepalpha[i] = -1.01 * dx[i] / (x[i] - alpha[i]);
        stepbeta[i] = 1.01 * dx[i] / (beta[i] - x[i]);
      }
      Real stmxx = *std::max_element(std::begin(stepxx), std::end(stepxx));
      Real stmalpha = *std::max_element(std::begin(stepalpha), std::end(stepalpha));
      Real stmbeta = *std::max_element(std::begin(stepbeta), std::end(stepbeta));
      Real stmalbe = std::max(stmalpha, stmbeta);
      Real stmalbexx = std::max(stmalbe, stmxx);
      Real stminv = std::max(stmalbexx, 1.0);
      Real steg = 1.0 / stminv;

      std::vector<Real> xold = x;
      Real yold = y;
      Real zold = z;
      Real lamold = lam;
      std::vector<Real> xsiold = xsi;
      std::vector<Real> etaold = eta;
      Real muold = mu;
      Real zetold = zet;
      Real sold = s;

      int itto = 0;
      Real resinew = 2 * residunorm;
      while (resinew > residunorm && itto < 50)
      {
        itto++;
        for (int i = 0; i < n; i++)
        {
          x[i] = xold[i] + steg * dx[i];
          xsi[i] = xsiold[i] + steg * dxsi[i];
          eta[i] = etaold[i] + steg * deta[i];
        }
        y = yold + steg * dy;
        z = zold + steg * dz;
        lam = lamold + steg * dlam;
        mu = muold + steg * dmu;
        zet = zetold + steg * dzet;
        s = sold + steg * ds;
        gvec = 0;
        for (int i = 0; i < n; i++)
        {
          ux1[i] = upp[i] - x[i];
          xl1[i] = x[i] - low[i];
          ux2[i] = ux1[i] * ux1[i];
          xl2[i] = xl1[i] * xl1[i];
          uxinv1[i] = een[i] / ux1[i];
          xlinv1[i] = een[i] / xl1[i];
          plam[i] = p0[i] + P[i] * lam;
          qlam[i] = q0[i] + Q[i] * lam;
          gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
          dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
          rex[i] = dpsidx[i] - xsi[i] + eta[i];
          rexsi[i] = xsi[i] * (x[i] - alpha[i]) - epsvecn[i];
          reeta[i] = eta[i] * (beta[i] - x[i]) - epsvecn[i];
        }
        rey = c + d * y - mu - lam;
        rez = a0 - zet - a * lam;
        relam = gvec - a * z - y + s - b;
        remu = mu * y - epsvecm;
        rezet = zet * z - epsi;
        res = lam * s - epsvecm;

        residu1.assign(std::begin(rex), std::end(rex));
        residu1.push_back(rey);
        residu1.push_back(rez);

        residu2.assign(1, relam);
        residu2.insert(std::end(residu2), std::begin(rexsi), std::end(rexsi));
        residu2.insert(std::end(residu2), std::begin(reeta), std::end(reeta));
        residu2.insert(std::end(residu2), {remu, rezet, res});

        residu.assign(std::begin(residu1), std::end(residu1));
        residu.insert(std::end(residu), std::begin(residu2), std::end(residu2));

        resinew = NormVec(residu);
        steg /= 2;
      }
      residunorm = resinew;
      std::vector<Real> residuabs = AbsVec(residu);
      residumax = *std::max_element(std::begin(residuabs), std::end(residuabs));
    }
    epsi *= 0.1;
  }
  return x;
}

std::vector<Real>
DensityUpdateCustom::AbsVec(std::vector<Real> vector)
{
  for (unsigned int i = 0; i < vector.size(); i++)
  {
    if (vector[i] < 0)
      vector[i] *= -1;
  }
  return vector;
}

Real
DensityUpdateCustom::NormVec(std::vector<Real> vector)
{
  Real accum = 0.0;
  for (unsigned int i = 0; i < vector.size(); i++)
  {
    accum += vector[i] * vector[i];
  }
  return std::sqrt(accum);
}

std::vector<Real>
DensityUpdateCustom::DensityFilter(std::vector<Real> density)
{
  unsigned int n_el = density.size();
  std::vector<Real> filt_density(n_el, 0);
  for (unsigned int i = 0; i < n_el; i++)
  {
    for (unsigned int j = 0; j < n_el; j++)
    {
      filt_density[i] += _H[i][j] * density[j];
    }
    filt_density[i] /= _Hs[i];
  }
  return filt_density;
}

MooseEnum
DensityUpdateCustom::getUpdateSchemeEnum()
{
  auto filter = MooseEnum("OC MMA", "OC");
  return filter;
}
