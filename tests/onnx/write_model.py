#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 CERN
#
# SPDX-License-Identifier: Apache-2.0

import onnx
from onnx import TensorProto, helper


def main():
    out_path = "identity_1x1.onnx"

    x = helper.make_tensor_value_info("X", TensorProto.FLOAT, [1, 1])
    y = helper.make_tensor_value_info("Y", TensorProto.FLOAT, [1, 1])
    node = helper.make_node("Identity", inputs=["X"], outputs=["Y"])
    graph = helper.make_graph([node], "identity_graph", [x], [y])
    model = helper.make_model(
        graph,
        producer_name="coopsync_tbb",
        opset_imports=[helper.make_opsetid("", 13)],
    )
    model.ir_version = 7

    onnx.checker.check_model(model)
    onnx.save_model(model, out_path)
    print(out_path)


if __name__ == "__main__":
    main()
