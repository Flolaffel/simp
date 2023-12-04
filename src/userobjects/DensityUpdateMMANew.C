//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateMMANew.h"
#include <algorithm>

#include "Output.h"

registerMooseObject("OptimizationApp", DensityUpdateMMANew);

InputParameters
DensityUpdateMMANew::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Compute updated densities based on sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredCoupledVar("old_design_density1",
                               "Design density one iteration ago variable name.");
  params.addRequiredCoupledVar("old_design_density2",
                               "Design density two iterations ago variable name.");
  params.addRequiredParam<VariableName>("density_sensitivity",
                                        "Name of the density_sensitivity variable.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addRequiredCoupledVar("mma_lower_asymptotes",
                               "Column vector with the lower asymptotes from the previous "
                               "iteration (provided that iter>1).");
  params.addRequiredCoupledVar("mma_upper_asymptotes",
                               "Column vector with the upper asymptotes from the previous "
                               "iteration (provided that iter>1).");
  params.addParam<int>(
      "execution_order_group",
      1,
      "Execution order groups are executed in increasing order (e.g., the lowest "
      "number is executed first). Note that negative group numbers may be used to execute groups "
      "before the default (0) group. Please refer to the user object documentation "
      "for ordering of user object execution within a group.");
  return params;
}

DensityUpdateMMANew::DensityUpdateMMANew(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _mesh(_subproblem.mesh()),
    _density_sensitivity_name(getParam<VariableName>("density_sensitivity")),
    _design_density(&writableVariable("design_density")),
    _old_design_density1(&writableVariable("old_design_density1")),
    _old_design_density2(&writableVariable("old_design_density2")),
    _density_sensitivity(&_subproblem.getStandardVariable(_tid, _density_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _lower_asymptotes(&writableVariable("mma_lower_asymptotes")),
    _upper_asymptotes(&writableVariable("mma_upper_asymptotes"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
  if (!dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1))
    paramError("old_design_density1",
               "Design density one it ago must be a finite element variable");
  if (!dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2))
    paramError("old_design_density2",
               "Design density two its ago must be a finite element variable");
}

void
DensityUpdateMMANew::initialize()
{
  gatherElementData();
  performMMALoop();
}

void
DensityUpdateMMANew::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)->setNodalValue(elem_data.new_density);
    dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)
        ->setNodalValue(elem_data.current_density);
    dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)
        ->setNodalValue(elem_data.old_density1);
    dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->setNodalValue(elem_data.new_lower);
    dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->setNodalValue(elem_data.new_upper);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateMMANew::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_density_sensitivity)
              ->getElementalValue(elem),
          elem->volume(),
          dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->getElementalValue(elem),
          0,
          0,
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateMMANew::performMMALoop()
{
  int m = 1;
  int n = _elem_data_map.size();

  // Valarray variables of size n
  std::valarray<Real> xval(n), xold1(n), xold2(n), low(n), upp(n), df0dx(n), xmin(n), xmax(1, n);
  std::valarray<Real> dfdx(1.0 / (_volume_fraction * n), n);

  // Scalar constants for the case of one constraint (m=1)
  Real a0 = 1;
  Real a = 0;
  Real c_MMA = 10000;
  Real d = 0;
  Real fval = 0;

  for (auto && [id, elem_data] : _elem_data_map)
  {
    xval[id] = elem_data.current_density;
    xold1[id] = elem_data.old_density1;
    xold2[id] = elem_data.old_density2;
    df0dx[id] = elem_data.sensitivity;
    low[id] = elem_data.lower;
    upp[id] = elem_data.upper;
    fval += elem_data.current_density;
  }
  fval /= _volume_fraction * n;
  fval -= 1;

  // MMA

  // die hier spaeter als Parameter übergeben
  Real epsimin = 0.0000001;
  Real raa0 = 0.00001;
  Real move = 0.5;
  Real albefa = 0.1;
  Real asyinit = 0.5;
  Real asyincr = 1.2;
  Real asydecr = 0.7;
  std::valarray<Real> eeen(1, n);
  std::valarray<Real> eeem(1, m);

  // Calculation of the asymptotes low and upp
  std::valarray<Real> zzz;
  if (_t_step <= 2)
  {
    low = xval - asyinit * (xmax - xmin);
    upp = xval + asyinit * (xmax - xmin);
  }
  else
  {
    zzz = (xval - xold1) * (xold1 - xold2);
    std::valarray<Real> factor(n);
    for (int i = 0; i < n; i++)
    {
      if (zzz[i] > 0)
        factor[i] = asyincr;
      else
        factor[i] = asydecr;
    }
    low = xval - factor * (xold1 - low);
    upp = xval + factor * (upp - xold1);
    std::valarray<Real> lowmin, lowmax, uppmin, uppmax;
    lowmin = xval - 10 * (xmax - xmin);
    lowmax = xval - 0.01 * (xmax - xmin);
    uppmin = xval + 0.01 * (xmax - xmin);
    uppmax = xval + 10 * (xmax - xmin);
    low = maxValArray(low, lowmin);
    low = minValArray(low, lowmax);
    upp = minValArray(upp, uppmax);
    upp = maxValArray(upp, uppmin);
  }

  // Calculation of the bounds alpha and beta
  std::valarray<Real> alpha, beta, zzz1, zzz2;
  zzz1 = low + albefa * (xval - low);
  zzz2 = xval - move * (xmax - xmin);
  zzz = maxValArray(zzz1, zzz2);
  alpha = maxValArray(zzz, xmin);
  zzz1 = upp - albefa * (upp - xval);
  zzz2 = xval + move * (xmax - xmin);
  zzz = minValArray(zzz1, zzz2);
  beta = minValArray(zzz, xmax);

  // Calculations of p0, q0, P, Q and b
  std::valarray<Real> xmami, xmamieps, xmamiinv, ux1, ux2, xl1, xl2, uxinv, xlinv;
  xmami = xmax - xmin;
  xmamieps = 0.00001 * eeen;
  xmami = maxValArray(xmami, xmamieps);
  xmamiinv = eeen / xmami;
  ux1 = upp - xval;
  ux2 = ux1 * ux1;
  xl1 = xval - low;
  xl2 = xl1 * xl1;
  uxinv = eeen / ux1;
  xlinv = eeen / xl1;

  std::valarray<Real> p0, q0, pq0;
  std::valarray<Real> zeron(n);
  p0 = maxValArray(df0dx, zeron);
  q0 = maxValArray(-df0dx, zeron);
  // for (int i = 0; i < n; i++)
  // {
  //   std::cout << p0[i] << " | " << q0[i] << std::endl;
  // }
  pq0 = 0.001 * (p0 + q0) + raa0 * xmamiinv;
  p0 = p0 + pq0;
  q0 = q0 + pq0;
  p0 = p0 * ux2;
  q0 = q0 * xl2;

  std::valarray<Real> P, Q, PQ;
  P = maxValArray(dfdx, zeron);
  Q = maxValArray(-dfdx, zeron);
  PQ = 0.001 * (P + Q) + raa0 * eeem[0] * xmamiinv;
  P = P + PQ;
  Q = Q + PQ;
  P = P * ux2;
  Q = Q * xl2;
  Real b = 0;
  for (int i = 0; i < n; i++)
  {
    b += P[i] * uxinv[i] + Q[i] * xlinv[i];
    // low und upp ändern sich nicht, da xval sich nicht ändert
    // std::cout << i << "4: " << low[i] << " | " << upp[i] << " | " << alpha[i] << " | " << beta[i]
    //           << " | " << p0[i] << " | " << q0[i] << " | " << P[i] << " | " << Q[i] << " | "
    //           << uxinv[i] << " | " << xlinv[i] << " | " << b << std::endl;
  }
  b -= fval;
  // std::cout << "1: " << b << " | " << fval << std::endl;
  // Solving the subproblem by a primal-dual Newton method
  std::valarray<Real> new_density =
      MMASubSolve(m, n, epsimin, low, upp, alpha, beta, p0, q0, P, Q, a0, a, b, c_MMA, d);

  for (auto && [id, elem_data] : _elem_data_map)
  {
    // Update the current filtered density for the current element
    elem_data.new_density = new_density[id];
    elem_data.new_lower = low[id];
    elem_data.new_upper = upp[id];
  }
}

// Method to solve the MMA subproblem by a primal-dual Newton method
std::valarray<Real>
DensityUpdateMMANew::MMASubSolve(int m,
                                 int n,
                                 Real epsimin,
                                 std::valarray<Real> low,
                                 std::valarray<Real> upp,
                                 std::valarray<Real> alpha,
                                 std::valarray<Real> beta,
                                 std::valarray<Real> p0,
                                 std::valarray<Real> q0,
                                 std::valarray<Real> P,
                                 std::valarray<Real> Q,
                                 Real a0,
                                 Real a,
                                 Real b,
                                 Real c,
                                 Real d)
{
  std::valarray<Real> een(1, n);
  Real eem = 1.0;
  Real epsi = 1.0;
  std::valarray<Real> epsvecn = epsi * een;
  Real epsvecm = epsi * eem;
  std::valarray<Real> x = 0.5 * (alpha + beta);
  Real y = eem;
  Real z = 1.0;
  Real lam = eem;
  std::valarray<Real> xsi = een / (x - alpha);
  xsi = maxValArray(xsi, een);
  std::valarray<Real> eta = een / (beta - x);
  eta = maxValArray(eta, een);
  Real mu = std::max(eem, 0.5 * c);
  Real zet = 1.0;
  Real s = eem;
  while (epsi > epsimin)
  {
    epsvecn = epsi * een;
    epsvecm = epsi * eem;
    std::valarray<Real> ux1 = upp - x;
    std::valarray<Real> xl1 = x - low;
    std::valarray<Real> ux2 = ux1 * ux1;
    std::valarray<Real> xl2 = xl1 * xl1;
    std::valarray<Real> uxinv1 = een / ux1;
    std::valarray<Real> xlinv1 = een / xl1;
    std::valarray<Real> plam = p0 + P * lam;
    std::valarray<Real> qlam = q0 + Q * lam;
    Real gvec = 0;
    for (int i = 0; i < n; i++)
    {
      gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
    }
    std::valarray<Real> dpsidx = plam / ux2 - qlam / xl2;
    std::valarray<Real> rex = dpsidx - xsi + eta;
    Real rey = c + d * y - mu - lam;
    Real rez = a0 - zet - a * lam;
    Real relam = gvec - a * z - y + s - b;
    std::valarray<Real> rexsi = xsi * (x - alpha) - epsvecn;
    std::valarray<Real> reeta = eta * (beta - x) - epsvecn;
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

    Real residunorm = NormVector(residu);
    std::vector<Real> residuabs = AbsVec(residu);
    Real residumax = *max_element(std::begin(residuabs), std::end(residuabs));
    int ittt = 0;
    while (residumax > 0.9 * epsi && ittt < 200)
    {
      ittt++;
      ux1 = upp - x;
      xl1 = x - low;
      ux2 = ux1 * ux1;
      xl2 = xl1 * xl1;
      std::valarray<Real> ux3 = ux1 * ux2;
      std::valarray<Real> xl3 = xl1 * xl2;
      uxinv1 = een / ux1;
      xlinv1 = een / xl1;
      std::valarray<Real> uxinv2 = een / ux2;
      std::valarray<Real> xlinv2 = een / xl2;
      plam = p0 + P * lam;
      qlam = q0 + Q * lam;
      gvec = 0;
      for (int i = 0; i < n; i++)
      {
        gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
      }
      std::valarray<Real> GG = P * uxinv2 - Q * xlinv2;
      dpsidx = plam / ux2 - qlam / xl2;
      std::valarray<Real> delx = dpsidx - epsvecn / (x - alpha) + epsvecn / (beta - x);
      Real dely = c + d * y - lam - epsvecm / y;
      Real delz = a0 - a * lam - epsi / z;
      Real dellam = gvec - a * z - y - b + epsvecm / lam;
      std::valarray<Real> diagx = plam / ux3 + qlam / xl3;
      diagx = 2 * diagx + xsi / (x - alpha) + eta / (beta - x);
      std::valarray<Real> diagxinv = een / diagx;
      Real diagy = d + mu / y;
      Real diagyinv = eem / diagy;
      Real diaglam = s / lam;
      Real diaglamyi = diaglam + diagyinv;

      std::valarray<Real> dx;
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
        std::vector<Real> AAinv{factor * AA[3], -factor * AA[1], -factor * AA[2], factor * AA[0]};
        std::vector<Real> solut{AAinv[0] * bb[0] + AAinv[1] * bb[1],
                                AAinv[2] * bb[0] + AAinv[3] * bb[1]};
        dlam = solut[0];
        dz = solut[1];
        dx = -delx / diagx - (GG * dlam) / diagx;
      }
      else
      {
        // idk
      }

      Real dy = -dely / diagy + dlam / diagy;
      std::valarray<Real> dxsi = -xsi + epsvecn / (x - alpha) - (xsi * dx) / (x - alpha);
      std::valarray<Real> deta = -eta + epsvecn / (beta - x) + (eta * dx) / (beta - x);
      Real dmu = -mu + epsvecm / y - (mu * dy) / y;
      Real dzet = -zet + epsi / z - zet * dz / z;
      Real ds = -s + epsvecm / lam - (s * dlam) / lam;

      std::vector<Real> xx{y, z, lam};
      xx.reserve(xx.size() + xsi.size() + eta.size() + 3);
      xx.insert(std::end(xx), std::begin(xsi), std::end(xsi));
      xx.insert(std::end(xx), std::begin(eta), std::end(eta));
      xx.insert(std::end(xx), {mu, zet, s});

      std::vector<Real> dxx{dy, dz, dlam};
      dxx.reserve(dxx.size() + xsi.size() + eta.size() + 3);
      dxx.insert(std::end(dxx), std::begin(dxsi), std::end(dxsi));
      dxx.insert(std::end(dxx), std::begin(deta), std::end(deta));
      dxx.insert(std::end(dxx), {dmu, dzet, ds});

      std::vector<Real> stepxx(xx.size());
      for (unsigned int i = 0; i < xx.size(); i++)
      {
        stepxx[i] = -1.01 * dxx[i] / xx[i];
      }
      std::vector<Real> stepalpha(n), stepbeta(n);
      for (int i = 0; i < n; i++)
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

      std::valarray<Real> xold = x;
      Real yold = y;
      Real zold = z;
      Real lamold = lam;
      std::valarray<Real> xsiold = xsi;
      std::valarray<Real> etaold = eta;
      Real muold = mu;
      Real zetold = zet;
      Real sold = s;

      int itto = 0;
      Real resinew = 2 * residunorm;
      while (resinew > residunorm && itto < 50)
      {
        itto++;

        x = xold + steg * dx;

        // for (int i = 0; i < n; i++)
        // {
        //   if (i % 100 == 0)
        //     std::cout << x[i] << " | " << steg << " | " << dx[i] << std::endl;
        // }

        y = yold + steg * dy;
        z = zold + steg * dz;
        lam = lamold + steg * dlam;
        xsi = xsiold + steg * dxsi;
        eta = etaold + steg * deta;
        mu = muold + steg * dmu;
        zet = zetold + steg * dzet;
        s = sold + steg * ds;
        ux1 = upp - x;
        xl1 = x - low;
        ux2 = ux1 * ux1;
        xl2 = xl1 * xl1;
        uxinv1 = een / ux1;
        xlinv1 = een / xl1;
        plam = p0 + P * lam;
        qlam = q0 + Q * lam;
        gvec = 0;
        for (int i = 0; i < n; i++)
        {
          gvec += P[i] * uxinv1[i] + Q[i] * xlinv1[i];
        }
        dpsidx = plam / ux2 - qlam / xl2;
        rex = dpsidx - xsi + eta;
        rey = c + d * y - mu - lam;
        rez = a0 - zet - a * lam;
        relam = gvec - a * z - y + s - b;
        rexsi = xsi * (x - alpha) - epsvecn;
        reeta = eta * (beta - x) - epsvecn;
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

        resinew = NormVector(residu);
        steg /= 2;
      }
      residunorm = resinew;
      std::vector<Real> residuabs = AbsVec(residu);
      residumax = *std::max_element(std::begin(residuabs), std::end(residuabs));
    }
    epsi *= 0.1;
  }
  for (int i = 0; i < n; i++)
  {
    if (i % 100 == 0)
      std::cout << x[i] << std::endl;
  }
  return x;
}

std::vector<Real>
DensityUpdateMMANew::AbsVec(std::vector<Real> vector)
{
  for (unsigned int i = 0; i < vector.size(); i++)
  {
    if (vector[i] < 0)
      vector[i] *= -1;
  }
  return vector;
}

Real
DensityUpdateMMANew::NormVector(std::vector<Real> vector)
{
  Real accum = 0.0;
  for (unsigned int i = 0; i < vector.size(); i++)
  {
    accum += vector[i] * vector[i];
  }
  return std::sqrt(accum);
}

std::valarray<Real>
DensityUpdateMMANew::maxValArray(std::valarray<Real> val1, std::valarray<Real> val2)
{
  // Check for same size
  if (val1.size() != val2.size())
  {
    std::cout << "Valarrays have to be of same size" << std::endl;
    return val1;
  }

  for (unsigned int i = 0; i < val1.size(); i++)
  {
    if (val1[i] > val2[i])
      continue;
    else
      val1[i] = val2[i];
  }
  return val1;
}

std::valarray<Real>
DensityUpdateMMANew::minValArray(std::valarray<Real> val1, std::valarray<Real> val2)
{
  // Check for same size
  if (val1.size() != val2.size())
  {
    std::cout << "Valarrays have to be of same size" << std::endl;
    return val1;
  }

  for (unsigned int i = 0; i < val1.size(); i++)
  {
    if (val1[i] < val2[i])
      continue;
    else
      val1[i] = val2[i];
  }
  return val1;
}
