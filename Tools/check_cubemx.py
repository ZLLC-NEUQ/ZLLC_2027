#!/usr/bin/env python3
"""Check that CubeMX regeneration preserved native application/build ownership.

Run after generation: python Tools/check_cubemx.py [--root PROJECT_ROOT]
Uses only the Python 3.6+ standard library. Checks source/configuration rather
than build artifacts, and includes the existing interrupt/task priority audit.
"""

import argparse
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from check_priorities import (AuditError, audit as audit_priorities, calls,
                              functions, properties, read, require, uncomment)


def user_regions(text, relative):
    """Validate CubeMX markers and return the preserved text by region name."""
    regions, opened = {}, None
    pattern = r"/\*\s*USER CODE (BEGIN|END)\s+([^*]+?)\s*\*/"
    for match in re.finditer(pattern, text):
        action, name = match.groups()
        name = name.strip()
        line = text.count("\n", 0, match.start()) + 1
        context = "%s:%d" % (relative, line)
        if action == "BEGIN":
            require(opened is None, context + ": nested USER CODE marker; restore the CubeMX markers")
            require(name not in regions, context + ": duplicate USER CODE region " + name)
            opened = (name, match.end())
        else:
            require(opened is not None and opened[0] == name,
                    context + ": unmatched USER CODE END " + name + "; restore the CubeMX markers")
            regions[name] = text[opened[1]:match.start()]
            opened = None
    require(opened is None, relative + ": USER CODE region has no END marker")
    return regions


def audit_sources(root):
    config_dir = root / "User/Task/Inc"
    for name in ("FreeRTOSConfig.h", "priority_config.h"):
        require((config_dir / name).is_file(),
                "Missing User/Task/Inc/" + name + "; restore the application-owned configuration")
        require(not (root / "Core/Inc" / name).exists(),
                "Remove duplicate Core/Inc/" + name + "; it shadows the application-owned configuration")

    generated = {}
    for directory in ("Core", "USB_DEVICE"):
        for path in sorted((root / directory).rglob("*")):
            if path.suffix.lower() in (".c", ".h"):
                relative = path.relative_to(root).as_posix()
                text = read(path)
                generated[relative] = (text, user_regions(text, relative))
    require("Core/Src/main.c" in generated, "Missing generated Core/Src/main.c")
    main_text, regions = generated["Core/Src/main.c"]
    require("2" in regions, "Core/Src/main.c: restore USER CODE region 2 for application startup")
    startup = uncomment(regions["2"])
    positions = []
    for name in ("Task_Init", "FreeRTOS_Start"):
        all_calls = list(calls(uncomment(main_text), name))
        protected_calls = list(calls(startup, name))
        require(len(all_calls) == len(protected_calls) == 1,
                "Core/Src/main.c: " + name + "() must appear exactly once inside USER CODE 2")
        require(protected_calls[0][0] == [""],
                "Core/Src/main.c: unexpected arguments to " + name + "()")
        positions.append(protected_calls[0][1])
    require(positions[0] < positions[1],
            "Core/Src/main.c: call Task_Init() before FreeRTOS_Start() in USER CODE 2")

    owned_handlers = {"SVC_Handler", "PendSV_Handler", "SysTick_Handler"}
    for relative, (text, _) in generated.items():
        if relative.startswith("Core/") and relative.endswith(".c"):
            defined = {name for name, _, _ in functions(uncomment(text))}
            conflicts = defined & (owned_handlers | {"HAL_InitTick"})
            require(not conflicts,
                    relative + ": generated code defines " + ", ".join(sorted(conflicts)) +
                    "; disable CubeMX RTOS exception/timebase generation and restore native ownership")
    timer = uncomment(read(root / "User/Driver/Src/drv_tim.cpp"))
    require(sum(name == "HAL_InitTick" for name, _, _ in functions(timer)) == 1,
            "User/Driver/Src/drv_tim.cpp: restore the single custom HAL_InitTick() TIM6 implementation")
    config = uncomment(read(config_dir / "FreeRTOSConfig.h"))
    for alias, handler in (("vPortSVCHandler", "SVC_Handler"),
                           ("xPortPendSVHandler", "PendSV_Handler"),
                           ("xPortSysTickHandler", "SysTick_Handler")):
        definitions = [value.strip() for value in
                       re.findall(r"^\s*#\s*define\s+" + alias + r"\s+([^\n]+)", config, re.M)]
        require(definitions == [handler],
                "User/Task/Inc/FreeRTOSConfig.h: restore #define " + alias + " " + handler)
    print("PASS: %d generated C/H files have valid USER CODE markers; native startup/RTOS handlers are preserved." %
          len(generated))


def audit_ioc(root):
    ioc = properties(root / "ZLLC_2027.ioc")
    require(ioc.get("ProjectManager.KeepUserCode") == "true",
            "CubeMX Project Manager > Code Generator: enable Keep User Code when re-generating")
    enabled_ips = {value.upper() for key, value in ioc.items() if re.fullmatch(r"Mcu\.IP\d+", key)}
    require("FREERTOS" not in enabled_ips and not any(key.upper().startswith("FREERTOS.") for key in ioc),
            "Disable CubeMX FREERTOS middleware; this project uses its existing native FreeRTOS configuration/tasks")
    require(ioc.get("VP_SYS_VS_Systick.Mode") == "SysTick" and
            ioc.get("VP_SYS_VS_Systick.Signal") == "SYS_VS_Systick",
            "CubeMX System Core > SYS: keep the SysTick timebase model; drv_tim.cpp owns the runtime TIM6 HAL tick")
    require(not any(key.startswith("VP_SYS_VS_TIM") for key in ioc),
            "Remove the CubeMX timer timebase selection; it conflicts with drv_tim.cpp HAL_InitTick()")
    audit_fdcan(root, ioc)
    print("PASS: CubeMX keeps USER CODE and leaves native FreeRTOS/TIM6 ownership intact.")


def audit_fdcan(root, ioc):
    source = uncomment(read(root / "Core/Src/fdcan.c"))
    header = uncomment(read(root / "Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal_fdcan.h"))
    modes = set(re.findall(r"^\s*#\s*define\s+(FDCAN_MODE_\w+)\b", header, re.M))
    for instance in (1, 2, 3):
        prefix = "FDCAN%d" % instance
        parameters = set(ioc.get(prefix + ".IPParameters", "").split(","))
        require({"Mode", "TxBuffersNbr"} <= parameters,
                prefix + ": explicitly configure Mode and TxBuffersNbr in CubeMX/IPParameters; defaults are unsafe")
        values = {}
        for field in ("Mode", "TxBuffersNbr", "TxFifoQueueElmtsNbr", "MessageRAMOffset"):
            key = prefix + "." + field
            require(key in ioc, "Missing " + key + "; explicitly save the FDCAN configuration in CubeMX")
            assignments = re.findall(r"\bhfdcan%d\.Init\.%s\s*=\s*([^;]+);" % (instance, field), source)
            require(len(assignments) == 1, "Core/Src/fdcan.c: expected one generated assignment for " + key)
            configured, generated = ioc[key].strip(), assignments[0].strip()
            if field == "Mode":
                require(configured in modes, key + ": select an explicit supported HAL FDCAN mode in CubeMX")
            else:
                try:
                    configured = int(configured, 0)
                    generated = int(re.sub(r"[uUlL]+$", "", generated), 0)
                except ValueError:
                    raise AuditError(key + ": expected an integer in both IOC and generated FDCAN initialization")
                require(configured >= 0, key + ": the FDCAN count/offset must not be negative")
                values[field] = configured
            require(configured == generated,
                    key + ": IOC/source mismatch; regenerate Core/Src/fdcan.c from the saved FDCAN configuration")
        require(values["TxBuffersNbr"] + values["TxFifoQueueElmtsNbr"] == 32,
                prefix + ": CubeMX H7 requires TxBuffersNbr + TxFifoQueueElmtsNbr = 32; fix both TX allocation fields")
    print("PASS: all 3 FDCAN modes/TX allocations/message RAM offsets match the explicit CubeMX model.")


def project_path(directory, value):
    return (directory / value.strip().strip('"').replace("\\", "/")).resolve()


def xml_number(node, path):
    value = node.findtext(path, "").strip()
    try:
        return int(value, 0)
    except ValueError:
        raise AuditError("Keil project: missing/non-integer " + path + ": " + value)


def audit_keil(root):
    project_file = root / "MDK-ARM/ZLLC_2027.uvprojx"
    project = ET.fromstring(read(project_file))
    targets = [target for target in project.findall("./Targets/Target")
               if target.findtext("TargetName") == "ZLLC_2027"]
    require(len(targets) == 1, "Keil project: restore the single ZLLC_2027 target")
    target = targets[0]
    require(target.findtext("uAC6") == "1",
            "Keil ZLLC_2027 target: select ARM Compiler 6; CubeMX reset the compiler selection")
    directory = project_file.parent
    sources, files = {}, []
    for group in target.findall("./Groups/Group"):
        group_enabled = group.findtext("./GroupOption/CommonProperty/IncludeInBuild", "1") != "0"
        for file_node in group.findall("./Files/File"):
            value = file_node.findtext("FilePath", "")
            if value:
                path = project_path(directory, value)
                files.append(path)
                enabled = group_enabled and file_node.findtext("./FileOption/CommonProperty/IncludeInBuild", "1") != "0"
                if enabled:
                    sources.setdefault(path, []).append(file_node)

    kernel = "Middlewares/Third_Party/FreeRTOS/Source/"
    required_sources = ["User/Task/Src/freertos_tasks.cpp", "User/Task/Src/tsk_config_and_callback.cpp",
                        "User/Driver/Src/drv_tim.cpp"]
    required_sources += [kernel + name + ".c" for name in
                         ("croutine", "event_groups", "list", "queue", "stream_buffer", "tasks", "timers")]
    required_sources += [kernel + "portable/MemMang/heap_4.c",
                         kernel + "portable/GCC/ARM_CM7/r0p1/port.c"]
    for relative in required_sources:
        path = (root / relative).resolve()
        require(path.is_file() and len(sources.get(path, [])) == 1,
                "Keil target: restore one enabled source entry for " + relative)
        require(sources[path][0].findtext("FileType") in ("1", "8"),
                "Keil target: mark " + relative + " as a compiled C/C++ source")
    includes = {project_path(directory, value)
                for node in target.findall("./TargetOption/TargetArmAds/Cads/VariousControls/IncludePath")
                for value in (node.text or "").split(";") if value.strip()}
    required_includes = ["User/" + name + "/Inc" for name in
                         ("Congfig", "Task", "Interaction", "Chariot", "Device", "Driver", "Algorithm")]
    required_includes += [kernel + "include", kernel + "portable/GCC/ARM_CM7/r0p1"]
    for relative in required_includes:
        require((root / relative).resolve() in includes,
                "Keil C/C++ Include Paths: restore ../" + relative)

    misc = target.find("./TargetOption/TargetArmAds/ArmAdsMisc")
    require(misc is not None, "Keil project: ARM target memory options are missing")
    require(xml_number(misc, "OnChipMemories/OCR_RVCT10/StartAddress") == 0x24000000 and
            xml_number(misc, "OnChipMemories/OCR_RVCT10/Size") == 0x50000 and
            misc.findtext("Im2Chk") == "1" and misc.findtext("RwSelD") == "4",
            "Keil target memory: restore selected AXI RAM at 0x24000000 with size 0x50000")

    dsp_source = any(component.get("Cclass") == "CMSIS" and component.get("Cgroup") == "DSP" and
                     component.get("Cvariant") == "Source" and
                     any(info.get("name") == "ZLLC_2027" for info in component.findall("./targetInfos/targetInfo"))
                     for component in project.findall("./RTE/components/component"))
    dsp_name = re.compile(r"(?:cmsis.*dsp|arm_cortex.*math)", re.I)
    dsp_library = any(path in sources and path.is_file() and path.suffix.lower() in (".a", ".lib") and
                      dsp_name.search(path.stem) for path in files)
    linker = target.find("./TargetOption/TargetArmAds/LDads")
    if linker is not None:
        library_dirs = [directory] + [project_path(directory, value) for value in
                                     re.split(r"[;,]", linker.findtext("IncludeLibsPath", "")) if value.strip()]
        for value in re.split(r"[;,]", linker.findtext("IncludeLibs", "")):
            if not value.strip():
                continue
            path = Path(value.strip().strip('"').replace("\\", "/"))
            if path.suffix.lower() in (".a", ".lib") and dsp_name.search(path.stem):
                dsp_library |= any((base / path).is_file() for base in library_dirs)
    require(dsp_source or dsp_library,
            "Keil Manage Run-Time Environment: restore CMSIS > DSP > Source, or link the existing CMSIS-DSP library")
    print("PASS: Keil retains ARM Compiler 6, native sources/includes, custom AXI RAM and CMSIS-DSP linkage.")


def audit(root):
    audit_sources(root)
    audit_ioc(root)
    audit_keil(root)
    audit_priorities(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        audit(args.root.resolve())
    except (AuditError, OSError, ET.ParseError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
