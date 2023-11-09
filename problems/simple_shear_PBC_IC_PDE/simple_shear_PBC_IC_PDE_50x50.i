nx = 50
ny = 50
p = 3
vol_frac = 0.5
E0 = 1
Emin = 1e-6

[Mesh]
  [domain]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -50
    xmax = 50
    ymin = -50
    ymax = 50
    nx = ${nx}
    ny = ${ny}
  []
  [mid]
    type = ExtraNodesetGenerator
    input = domain
    new_boundary = mid
    coord = '0 0 0'
  []
[]

[GlobalParams]
  displacements = 'u_x u_y'
[]

[Variables]
  [global_strain]
    order = THIRD
    family = SCALAR
  []
  [Dc]
    initial_condition = -1.0
  []
[]

[AuxVariables]
  [rho]
    family = MONOMIAL
    order = CONSTANT
  []
  [Dc_elem]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = -1.0
    [AuxKernel]
      type = SelfAux
      variable = Dc_elem
      v = Dc
      execute_on = 'TIMESTEP_END'
    []
  []
  [p]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${p}
  []
  [Emin]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${Emin}
  []
  [E0]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${E0}
  []
  [sensitivity]
    family = MONOMIAL
    order = FIRST
    initial_condition = -1.0
    [AuxKernel]
      type = MaterialRealAux
      variable = sensitivity
      property = sensitivity
      execute_on = LINEAR
    []
  []
[]

[Modules]
  [TensorMechanics]
    [Master]
      [stress_div]
        strain = SMALL
        add_variables = true
        global_strain = global_strain
        generate_output = 'strain_xx strain_xy strain_yy stress_xx stress_xy
                           stress_yy vonmises_stress'
      []
    []
    [GlobalStrain]
      [global_strain]
        scalar_global_strain = global_strain
        applied_stress_tensor = '0 0 0 0 0 1e-3'
        displacements = 'u_x u_y'
        auxiliary_displacements = 'disp_x disp_y'
        global_displacements = 'ug_x ug_y'
      []
    []
  []
[]

[Kernels]
  [diffusion]
    type = FunctionDiffusion
    variable = Dc
    function = 0.15 # radius coeff
  []
  [potential]
    type = Reaction
    variable = Dc
  []
  [source]
    type = CoupledForce
    variable = Dc
    v = sensitivity
  []
[]

[ICs]
  [rho_IC]
    type = BoundingBoxIC
    variable = rho
    x1 = -5
    x2 = 5.1
    y1 = -5
    y2 = 5.1
    inside = 0.001
    outside = ${vol_frac}
  []
[] 

[BCs]
  [Periodic]
    [all]
      auto_direction = 'x y'
      variable = ' u_x u_y'
    []
  []
  [fix_x]
    type = DirichletBC
    boundary = mid
    variable = u_x
    value = 0
  []
  [fix_y]
    type = DirichletBC
    boundary = mid
    variable = u_y
    value = 0
  []
  #[boundary_penalty]
  #  type = ADRobinBC
  #  variable = Dc
  #  boundary = 'left right bottom top'
  #  coefficient = 10
  #[]
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensor
    youngs_modulus = E_phys
    poissons_ratio = 0.3
    args = 'Emin rho p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rho ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rho'
    property_name = E_phys
  []
  [dc]
    type = ComplianceSensitivity
    design_density = rho
    youngs_modulus = E_phys
  []
  [compute_stress]
    type = ComputeLinearElasticStress
  []
[]

[UserObjects]
  [update]
    type = DensityUpdate
    density_sensitivity = Dc
    design_density = rho
    volume_fraction = ${vol_frac}
    execute_on = TIMESTEP_BEGIN
    force_postaux = true
  []
  [opt_conv]
    type = Terminator
    expression = 'stop_vol < 1e-3 & SE_stop < 1e-3'
    execute_on = TIMESTEP_END
  []
[]

[Preconditioning]
  [SMP]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = PJFNK
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -pc_factor_shift_type -pc_factor_shift_amount'
  petsc_options_value = 'lu       superlu_dist                  NONZERO               1e-15'
  dt = 1.0
  num_steps = 500
  nl_abs_tol = 1e-4
[]

[Outputs]
  exodus = true
[]

[Postprocessors]
  [total_vol]
    type = ElementIntegralVariablePostprocessor
    variable = rho
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [vol_change]
    type = ChangeOverTimePostprocessor
    postprocessor = total_vol
    change_with_respect_to_initial = false
    execute_on = 'initial timestep_end'
  []
  [n_el]
    type = ConstantPostprocessor
    value = ${fparse nx*ny}
  []
  [stop_vol]
    type = ParsedPostprocessor
    function = 'abs(vol_change/n_el)'
    pp_names = 'vol_change n_el'
  []
  [SE]
    type = ElementIntegralMaterialProperty
    mat_prop = strain_energy_density
  []
  [SE_stop]
    type = ChangeOverTimePostprocessor
    postprocessor = SE
    change_with_respect_to_initial = false
    compute_relative_change = true
    take_absolute_value = true
    execute_on = 'initial timestep_end'
  []
[]
