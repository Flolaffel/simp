[Constraints]
[lr1_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '3 4 0'
weights = '1 1 -1'
secondary_node_ids = 5
penalty = 1000000.0
formulation = kinematic
[]
[lr1_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '3 4 0'
weights = '1 1 -1'
secondary_node_ids = 5
penalty = 1000000.0
formulation = kinematic
[]
[lr2_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '7 4 0'
weights = '1 1 -1'
secondary_node_ids = 8
penalty = 1000000.0
formulation = kinematic
[]
[lr2_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '7 4 0'
weights = '1 1 -1'
secondary_node_ids = 8
penalty = 1000000.0
formulation = kinematic
[]
[bt1_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '1 7 0'
weights = '1 1 -1'
secondary_node_ids = 6
penalty = 1000000.0
formulation = kinematic
[]
[bt1_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '1 7 0'
weights = '1 1 -1'
secondary_node_ids = 6
penalty = 1000000.0
formulation = kinematic
[]
[]
