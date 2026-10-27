#!/usr/bin/env python3
"""Compare the legacy and TCM EKF paths using the actual algorithm/CMSIS sources.

Run with Python 3.6+ and a Windows MinGW C++ compiler (32-bit long, as on Arm):
    python Tools/check_ekf_tcm.py [--cxx C:/mingw64/bin/g++.exe]

Only the CMSIS type declarations and unused MCU header are stubbed. The matrix
implementations come from this repository, including the mode-1 implementation
included by kalman_filter.cpp. This checks numerical and workspace behavior, not STM32
timing or numerical identity with a separately built vendor library/component.
All compilation products are created in a temporary directory.
"""

import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


MATRIX_HEADER = r"""
#ifndef EKF_TEST_ARM_MATH_H
#define EKF_TEST_ARM_MATH_H
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef float float32_t;
typedef enum {
    ARM_MATH_SUCCESS = 0, ARM_MATH_ARGUMENT_ERROR = -1,
    ARM_MATH_LENGTH_ERROR = -2, ARM_MATH_SIZE_MISMATCH = -3,
    ARM_MATH_NANINF = -4, ARM_MATH_SINGULAR = -5
} arm_status;
typedef struct {
    uint16_t numRows, numCols;
    float32_t *pData;
} arm_matrix_instance_f32;
#ifdef __cplusplus
extern "C" {
#endif
void arm_mat_init_f32(arm_matrix_instance_f32 *, uint16_t, uint16_t, float32_t *);
arm_status arm_mat_add_f32(const arm_matrix_instance_f32 *, const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
arm_status arm_mat_sub_f32(const arm_matrix_instance_f32 *, const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
arm_status arm_mat_mult_f32(const arm_matrix_instance_f32 *, const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
arm_status arm_mat_trans_f32(const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
arm_status arm_mat_inverse_f32(const arm_matrix_instance_f32 *, arm_matrix_instance_f32 *);
#ifdef __cplusplus
}
#endif
#endif
"""


HARNESS = r"""
#include "QuaternionEKF.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static_assert(sizeof(long) == 4, "Use a compiler with Arm-compatible 32-bit long");
static_assert(sizeof(float) == 4, "The comparison expects 32-bit floats");

struct Range { const void *ptr; size_t bytes; };
static void check_independent(const KalmanFilter_t &a, const KalmanFilter_t &b)
{
    const Range ranges[] = {
        {a.MeasurementMap, 3}, {a.MeasurementDegree, 12},
        {a.MatR_DiagonalElements, 12}, {a.StateMinVariance, 24}, {a.temp, 3},
        {a.FilteredValue, 24}, {a.MeasuredVector, 12},
        {a.xhat_data, 24}, {a.xhatminus_data, 24}, {a.z_data, 12},
        {a.P_data, 144}, {a.Pminus_data, 144}, {a.F_data, 144}, {a.FT_data, 144},
        {a.H_data, 72}, {a.HT_data, 72}, {a.Q_data, 144}, {a.R_data, 36},
        {a.K_data, 72}, {a.S_data, 144}, {a.temp_matrix_data, 144},
        {a.temp_matrix_data1, 144}, {a.temp_vector_data, 24}, {a.temp_vector_data1, 24}
    };
    for (unsigned i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i) {
        assert(ranges[i].ptr != NULL);
        const uintptr_t start = (uintptr_t)ranges[i].ptr;
        for (unsigned j = 0; j < i; ++j) {
            const uintptr_t other = (uintptr_t)ranges[j].ptr;
            assert(start + ranges[i].bytes <= other || other + ranges[j].bytes <= start);
        }
    }
    assert(a.xhat_data != b.xhat_data);
    assert(a.P_data != b.P_data);
    assert(a.K_data != b.K_data);
    assert(a.temp_matrix_data != b.temp_matrix_data);
}

static void record(FILE *file, const float *values, size_t count)
{
    for (size_t i = 0; i < count; ++i) { assert(isfinite(values[i])); }
    assert(fwrite(values, sizeof(float), count, file) == count);
}

static void snapshot(FILE *file, const QEKF_INS_t &imu)
{
    record(file, imu.q, 4);
    record(file, imu.GyroBias, 3);
    record(file, imu.OrientationCosine, 3);
    const float angles[] = {imu.Roll, imu.Pitch, imu.Yaw, imu.YawTotalAngle,
                            imu.ChiSquare_Data[0]};
    record(file, angles, 5);
    record(file, imu.IMU_QuaternionEKF.xhat_data, 6);
    record(file, imu.IMU_QuaternionEKF.P_data, 36);
    record(file, imu.IMU_QuaternionEKF.K_data, 18);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    QEKF_INS_t first = {}, second = {};
#if EKF_USE_TCM
    // A failed claim must not change either the caller or the current owner.
    assert(Kalman_Filter_Init_TCM(NULL, 6, 0, 3) == 0);
    assert(Kalman_Filter_Init_TCM(&first.IMU_QuaternionEKF, 6, 0, 3) == 1);
    KalmanFilter_t original = first.IMU_QuaternionEKF;
    assert(Kalman_Filter_Init_TCM(&first.IMU_QuaternionEKF, 5, 0, 3) == 0);
    assert(memcmp(&original, &first.IMU_QuaternionEKF, sizeof(original)) == 0);
    KalmanFilter_t untouched = second.IMU_QuaternionEKF;
    assert(Kalman_Filter_Init_TCM(&second.IMU_QuaternionEKF, 6, 0, 3) == 0);
    assert(memcmp(&untouched, &second.IMU_QuaternionEKF, sizeof(untouched)) == 0);
    first.IMU_QuaternionEKF.P_data[35] = 123.0f;
    first.IMU_QuaternionEKF.MeasuredVector[2] = 456.0f;
    assert(Kalman_Filter_Init_TCM(&first.IMU_QuaternionEKF, 6, 0, 3) == 1);
    assert(first.IMU_QuaternionEKF.P_data == original.P_data);
    assert(first.IMU_QuaternionEKF.P_data[35] == 0.0f);
    assert(first.IMU_QuaternionEKF.MeasuredVector[2] == 0.0f);
#endif
    FILE *file = fopen(argv[1], "wb");
    assert(file != NULL);
    for (unsigned pass = 0; pass < 2; ++pass) {
        // Same instance addresses exercise reuse of the static workspace.
        memset(&first, 0, sizeof(first));
        memset(&second, 0, sizeof(second));
        IMU_QuaternionEKF_Init(10, 0.001f, 10000000, 0.9996f, 0.05f, 0.0022f, &first);
        IMU_QuaternionEKF_Init(10, 0.001f, 10000000, 0.9996f, 0.05f, 0.0022f, &second);
        check_independent(first.IMU_QuaternionEKF, second.IMU_QuaternionEKF);
        first.IMU_QuaternionEKF.xhat_data[5] = 0.001f;
        assert(second.IMU_QuaternionEKF.xhat_data[5] == 0.0f);
        for (unsigned i = 0; i < 2048; ++i) {
            const float t = (float)i * 0.001f;
            const float gx = i < 512 ? 0.0f : 0.06f * sinf(2.0f * t);
            const float gy = i < 512 ? 0.0f : 0.04f * cosf(1.5f * t);
            const float gz = i < 512 ? 0.0022f : 0.5f;
            const float ax = i < 1024 ? 0.0f : 0.8f * sinf(t);
            const float ay = i < 1024 ? 0.0f : -0.6f * cosf(t);
            const float az = (i >= 1536 && i < 1600) ? 13.0f : 9.81f;
            const float dt = i < 1536 ? 0.001f : 0.0009f + 0.00005f * (i % 5);
            IMU_QuaternionEKF_Update(gx, gy, gz, ax, ay, az, dt, &first);
            IMU_QuaternionEKF_Update(-gx, gy, -gz, -ax, ay, az, dt, &second);
            snapshot(file, first);
            snapshot(file, second);
        }
    }
    assert(fclose(file) == 0);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default=shutil.which("g++") or "C:/mingw64/bin/g++.exe")
    args = parser.parse_args()
    root = args.root.resolve()
    matrix_dir = root / "Drivers/CMSIS/DSP/Source/MatrixFunctions"
    sources = [root / "User/Algorithm/Src" / name
               for name in ("QuaternionEKF.cpp", "kalman_filter.cpp")]
    sources += [matrix_dir / ("arm_mat_%s_f32.c" % name)
                for name in ("init", "add", "sub", "trans")]
    with tempfile.TemporaryDirectory(prefix="ekf_tcm_check_") as directory:
        temporary = Path(directory)
        (temporary / "arm_math.h").write_text(MATRIX_HEADER, encoding="utf-8")
        (temporary / "stm32h7xx_hal.h").write_text("#include <stdint.h>\n", encoding="utf-8")
        harness = temporary / "check.cpp"
        harness.write_text(HARNESS, encoding="utf-8")
        outputs = []
        for mode in (0, 1):
            executable = temporary / ("mode%d.exe" % mode)
            command = [args.cxx, "-std=c++11", "-O0", "-fno-strict-aliasing",
                       "-DSTM32H723xx", "-DARM_MATH_DSP=1", "-DEKF_USE_TCM=%d" % mode,
                       "-I" + str(temporary), "-I" + str(root / "User/Algorithm/Inc"),
                       "-I" + str(root / "User/Congfig/Inc")]
            command += [str(path) for path in sources]
            if not mode:
                command += [str(matrix_dir / ("arm_mat_%s_f32.c" % name))
                            for name in ("mult", "inverse")]
            command += [str(harness), "-o", str(executable)]
            subprocess.check_call(command)
            output = temporary / ("mode%d.bin" % mode)
            subprocess.check_call([str(executable), str(output)])
            outputs.append(output.read_bytes())
        if outputs[0] != outputs[1]:
            count = len(outputs[0]) // 4
            legacy = struct.unpack("<%df" % count, outputs[0])
            tcm = struct.unpack("<%df" % count, outputs[1])
            error = max(abs(a - b) for a, b in zip(legacy, tcm))
            raise AssertionError("Legacy and TCM outputs differ; maximum absolute error: %g" % error)
        print("PASS: repository CMSIS source paths are bit-identical for 4096 updates per instance (2 instances).")
        print("PASS: workspace arrays do not overlap; extra instance fallback is independent.")
        print("PASS: TCM owner reinitialization and rejected claims preserve workspace ownership.")
        print("Compared %d finite floating-point values; STM32 timing still requires hardware." %
              (len(outputs[0]) // 4))


if __name__ == "__main__":
    main()
