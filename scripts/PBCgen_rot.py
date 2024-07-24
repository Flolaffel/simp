import itertools
import netCDF4 as ex
import csv
import numpy as np

# get MOOSE values
mooseFile = ex.Dataset(
    "problems/simple_shear_PBC_IC/stress_cons/simple_shear_PBC_IC_dens_minV_sc_20x20_rot_out.e"
)

encoded_nodeset_names = mooseFile.variables["ns_names"]
nodeset_names = []
for enc_name in encoded_nodeset_names:
    name = ""
    for char in enc_name:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    nodeset_names.append(name)

# NOTE Assumptions:
#   1. square domain
#   2. first four nodesets are side nodes (top, bottom, left, right)
num_node_sets = mooseFile.dimensions["num_node_sets"].size
nodesets = []
for i in range(num_node_sets):
    nodeset = mooseFile.variables["node_ns" + str(i + 1)][:]
    nodesets.append(nodeset)

# NOTE Assumptions:
#   1. nodesets are named "top", "bottom", "left", "right" (MOOSE default)
#   2. Nodeset names and actual nodesets have the same order
left_index = nodeset_names.index("left")
right_index = nodeset_names.index("right")
bottom_index = nodeset_names.index("bottom")
top_index = nodeset_names.index("top")

# Get coordinates of nodes and sort the sets accordingly
X = mooseFile.variables["coordx"][:]
Y = mooseFile.variables["coordy"][:]

tuple_nodesets = []
for nodeset in nodesets:
    tuple_nodeset = []
    for node in nodeset:
        node_tuple = (node, X[node - 1], Y[node - 1])
        tuple_nodeset.append(node_tuple)
    tuple_nodesets.append(tuple_nodeset)

# Sort left and right by y-coord
tuple_nodesets[left_index] = sorted(tuple_nodesets[left_index], key=lambda x: x[2])
tuple_nodesets[right_index] = sorted(tuple_nodesets[right_index], key=lambda x: x[2])
# Sort bottom and top by x-coord
tuple_nodesets[bottom_index] = sorted(tuple_nodesets[bottom_index], key=lambda x: x[1])
tuple_nodesets[top_index] = sorted(tuple_nodesets[top_index], key=lambda x: x[1])

mooseFile.close()

number_of_equation_pairs_lr = nodesets[left_index].size - 1
number_of_equation_pairs_bt = nodesets[bottom_index].size - 2
penalty = 1e6

with open("scripts/file.i", "w") as f:
    data = ["[Constraints]"]
    for i in range(number_of_equation_pairs_lr):
        tail = "[]"
        # NOTE Assumption: 2D
        displacements = ["disp_x", "disp_y"]
        for var in range(2):
            head = "[" + "lr" + str(i + 1) + "_" + displacements[var] + "]"
            body = [
                "type = LinearNodalConstraintFix",
                "variable = " + displacements[var],
                "primary = '{0} {1} {2}'".format(
                    tuple_nodesets[right_index][i + 1][0] - 1,
                    tuple_nodesets[left_index][0][0] - 1,
                    tuple_nodesets[right_index][0][0] - 1,
                ),
                "weights = '1 1 -1'",
                "secondary_node_ids = " + str(tuple_nodesets[left_index][i + 1][0] - 1),
                "penalty = " + str(penalty),
                "formulation = kinematic",
            ]
            sub_data = itertools.chain([head], body, [tail])
            data.extend(sub_data)

    for i in range(number_of_equation_pairs_bt):
        tail = "[]"
        # NOTE Assumption: 2D
        displacements = ["disp_x", "disp_y"]
        for var in range(2):
            head = "[" + "bt" + str(i + 1) + "_" + displacements[var] + "]"
            body = [
                "type = LinearNodalConstraintFix",
                "variable = " + displacements[var],
                "primary = '{0} {1} {2}'".format(
                    tuple_nodesets[bottom_index][i + 1][0] - 1,
                    tuple_nodesets[top_index][-1][0] - 1,
                    tuple_nodesets[bottom_index][-1][0] - 1,
                ),
                "weights = '1 1 -1'",
                "secondary_node_ids = " + str(tuple_nodesets[top_index][i + 1][0] - 1),
                "penalty = " + str(penalty),
                "formulation = kinematic",
            ]
            sub_data = itertools.chain([head], body, [tail])
            data.extend(sub_data)

    data.append("[]")

    for line in data:
        f.write(line + "\n")
