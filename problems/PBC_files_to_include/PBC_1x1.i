[Constraints]
[lr1_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '3 1 0'
weights = '1 1 -1'
secondary_node_ids = 2
penalty = 1000000.0
formulation = kinematic
[]
[lr1_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '3 1 0'
weights = '1 1 -1'
secondary_node_ids = 2
penalty = 1000000.0
formulation = kinematic
[]
[]
