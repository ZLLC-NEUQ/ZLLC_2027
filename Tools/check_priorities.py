#!/usr/bin/env python3
"""Audit the priority policy, generated sources and CubeMX model (stdlib only).

Run from any directory: python Tools/check_priorities.py [--root PROJECT_ROOT]
IOC SysTick deliberately models the HAL tick priority for CubeMX's generated
TICK_INT_PRIORITY. The strong HAL_InitTick routes that tick to TIM6; the
FreeRTOS port sets the actual SysTick and PendSV priorities to KERNEL instead.
Keep VP_SYS_VS_Systick: generating another TIM6 HAL_InitTick would duplicate it.

FromISR routes below are the reviewed HAL callback paths, not an inferred C
call graph. New callback owners/IRQ configurations fail until explicitly audited.
Integer expressions are parsed with ast, never eval; unsupported syntax fails.
"""

import argparse
import ast
import operator
from pathlib import Path
import re
import sys


class AuditError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise AuditError(message)


def read(path):
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    raise AuditError("Cannot decode source as UTF-8 or GB18030: " + str(path))


def uncomment(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*",
                  lambda m: "\n" * m.group().count("\n"), text, flags=re.S)


class Integers:
    OPS = {ast.Add: operator.add, ast.Sub: operator.sub,
           ast.Mult: operator.mul, ast.Div: operator.floordiv,
           ast.FloorDiv: operator.floordiv, ast.Mod: operator.mod,
           ast.LShift: operator.lshift, ast.RShift: operator.rshift,
           ast.BitOr: operator.or_, ast.BitAnd: operator.and_,
           ast.BitXor: operator.xor}

    def __init__(self, paths):
        self.definitions = {}
        self.cache = {}
        for path in paths:
            text = uncomment(re.sub(r"\\\r?\n", " ", read(path)))
            for match in re.finditer(r"^\s*#\s*define\s+(\w+)([^\n]*)", text, re.M):
                name, value = match.groups()
                if value.startswith("(") or not value.strip():
                    continue  # Function-like macro or header guard, not an integer.
                self.definitions.setdefault(name, []).append((value.strip(), path))

    def macro(self, name, stack=()):
        if name in self.cache:
            return self.cache[name]
        require(name not in stack, "Recursive priority macro: " + name)
        require(name in self.definitions, "Unknown priority identifier: " + name)
        values = []
        for expression, path in self.definitions[name]:
            values.append(self.expression(expression, str(path) + ": " + name,
                                          stack=stack + (name,)))
        require(len(set(values)) == 1, "Conflicting definitions of " + name)
        self.cache[name] = values[0]
        return values[0]

    def expression(self, expression, context, bindings=None, stack=()):
        bindings = bindings or {}
        normalized = re.sub(r"\b(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]+\b", r"\1", expression)
        normalized = re.sub(r"\(\s*(?:u?int(?:8|16|32|64)_t|UBaseType_t|TickType_t|size_t)\s*\)",
                            "", normalized)

        def visit(node):
            if hasattr(ast, "Constant"):
                if isinstance(node, ast.Constant) and type(node.value) is int:
                    return node.value
            elif isinstance(node, ast.Num) and type(node.n) is int:
                return node.n
            if isinstance(node, ast.Name):
                return bindings[node.id] if node.id in bindings else self.macro(node.id, stack)
            if isinstance(node, ast.BinOp) and type(node.op) in self.OPS:
                return self.OPS[type(node.op)](visit(node.left), visit(node.right))
            if isinstance(node, ast.UnaryOp):
                if isinstance(node.op, ast.USub):
                    return -visit(node.operand)
                if isinstance(node.op, ast.UAdd):
                    return visit(node.operand)
                if isinstance(node.op, ast.Invert):
                    return ~visit(node.operand)
            raise AuditError("Unsupported integer expression in " + context + ": " + expression)

        try:
            return visit(ast.parse(normalized.strip(), mode="eval").body)
        except (SyntaxError, TypeError, ValueError, ZeroDivisionError) as error:
            raise AuditError("Cannot parse integer expression in " + context + ": " +
                             expression + " (" + str(error) + ")")


def calls(text, name):
    """Yield argument expressions and positions; balance nested parentheses."""
    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", text):
        start = match.end()
        depth, quote, escaped, end = 1, None, False, start
        commas = []
        while end < len(text) and depth:
            char = text[end]
            if quote:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = None
            elif char in "\"'":
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            elif char == "," and depth == 1:
                commas.append(end)
            end += 1
        require(depth == 0, "Unbalanced call to " + name)
        if text[end:].lstrip().startswith("{"):
            continue  # Definition rather than a call.
        limits = [start - 1] + commas + [end - 1]
        args = [text[limits[i] + 1:limits[i + 1]].strip() for i in range(len(limits) - 1)]
        yield args, match.start()


def properties(path):
    result = {}
    for line in read(path).splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            key, value = line.split("=", 1)
            require(key not in result, "Duplicate IOC property: " + key)
            result[key] = value
    return result


def irq_fields(ioc, irq):
    key = "NVIC." + irq
    require(key in ioc, "Missing IOC priority: " + key)
    fields = ioc[key].split(r"\:")
    require(len(fields) == 9, "Unsupported IOC NVIC format: " + key)
    try:
        return fields[0], int(fields[1]), int(fields[2])
    except ValueError:
        raise AuditError("Non-integer IOC priority: " + key)


def functions(text):
    # Mask strings so braces in diagnostic messages cannot affect span detection.
    masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: " " * len(m.group()), text)
    pattern = r"\b([A-Za-z_]\w*(?:::\w+)?)\s*\([^;{}]*\)\s*\{"
    for match in re.finditer(pattern, masked):
        if match.group(1) in ("if", "while", "for", "switch"):
            continue
        depth, end = 1, match.end()
        while end < len(masked) and depth:
            depth += (masked[end] == "{") - (masked[end] == "}")
            end += 1
        require(depth == 0, "Unbalanced function: " + match.group(1))
        yield match.group(1), match.start(), end


# IRQ roles come from the reviewed peripheral wiring, values from the header.
UARTS = {"UART5": "huart5", "UART7": "huart7", "UART8": "huart8",
         "UART9": "huart9", "USART1": "huart1", "USART10": "huart10"}
DMA_STREAMS = {"SPI2_TX": "DMA1_Stream1", "SPI2_RX": "DMA1_Stream2",
               "UART5_RX": "DMA1_Stream0", "UART7_RX": "DMA1_Stream5",
               "UART8_RX": "DMA1_Stream6", "UART8_TX": "DMA1_Stream7",
               "UART9_RX": "DMA2_Stream0", "UART9_TX": "DMA1_Stream4",
               "USART1_RX": "DMA2_Stream1", "USART1_TX": "DMA2_Stream2",
               "USART10_RX": "DMA1_Stream3", "USART10_TX": "DMA2_Stream4"}


def channel_role(channel):
    if channel.startswith("SPI"):
        return "SENSOR"
    if channel.endswith("_TX"):
        return "UART_TX"
    return "CONTROL_RX" if channel in ("UART5_RX", "USART1_RX") else "COMM_RX"


def audit(root):
    ioc = properties(root / "ZLLC_2027.ioc")
    macros = Integers([root / "User/Task/Inc/priority_config.h", root / "User/Task/Inc/FreeRTOSConfig.h",
                       root / "Core/Inc/stm32h7xx_hal_conf.h",
                       root / "Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h723xx.h"])
    priority = lambda role: macros.macro("ROBOT_IRQ_PRIORITY_" + role)
    bits = macros.macro("configPRIO_BITS")
    minimum, kernel, tick = priority("RTOS_API_MIN"), priority("KERNEL"), priority("HAL_TICK")
    require(bits == 4 and kernel == (1 << bits) - 1, "Kernel must use the lowest implemented priority")
    require(0 < tick < minimum <= kernel, "HAL tick must preempt RTOS-capable peripheral IRQs")
    require(macros.macro("configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY") == minimum,
            "FreeRTOS ISR threshold differs from the policy")
    require(macros.macro("configLIBRARY_LOWEST_INTERRUPT_PRIORITY") == kernel,
            "FreeRTOS lowest priority differs from the policy")
    require(macros.macro("configMAX_SYSCALL_INTERRUPT_PRIORITY") == minimum << (8 - bits),
            "Incorrect shifted FreeRTOS ISR threshold")
    require(macros.macro("configKERNEL_INTERRUPT_PRIORITY") == kernel << (8 - bits),
            "Incorrect shifted kernel priority")
    require(macros.macro("ROBOT_IRQ_SUB_PRIORITY") == 0, "Subpriority must be zero")
    require(ioc.get("NVIC.PriorityGroup") == "NVIC_PRIORITYGROUP_4", "IOC must use GROUP4")

    roles = {stream + "_IRQn": channel_role(channel) for channel, stream in DMA_STREAMS.items()}
    roles.update({uart + "_IRQn": channel_role(uart + "_RX") for uart in UARTS})
    roles.update({"FDCAN%d_IT0_IRQn" % n: "CAN" for n in (1, 2, 3)})
    roles.update({"SPI2_IRQn": "SENSOR", "EXTI15_10_IRQn": "SENSOR", "OTG_HS_IRQn": "USB"})
    require(len(roles) == 24, "Internal IRQ wiring table is incomplete")
    expected = {irq: priority(role) for irq, role in roles.items()}
    expected["TIM6_DAC_IRQn"] = tick
    require(all(0 <= value <= kernel for value in expected.values()), "Peripheral priority exceeds implemented NVIC range")
    code = {}
    for directory in ("Core/Src", "User", "USB_DEVICE"):
        for path in (root / directory).rglob("*"):
            if path.suffix.lower() in (".c", ".cpp"):
                code[path.relative_to(root).as_posix()] = uncomment(read(path))
    settings, enabled, groups = {}, set(), []
    for relative, text in code.items():
        for args, position in calls(text, "HAL_NVIC_SetPriority"):
            context = relative + ":" + str(text.count("\n", 0, position) + 1)
            require(len(args) == 3, "Wrong NVIC call arity in " + context)
            irq = args[0]
            require(irq in expected, "Unaudited NVIC IRQ/expression in " + context + ": " + irq)
            bindings = {"TickPriority": tick} if irq == "TIM6_DAC_IRQn" else {}
            pre = macros.expression(args[1], context, bindings)
            sub = macros.expression(args[2], context, bindings)
            require((pre, sub) == (expected[irq], 0),
                    "%s: %s = (%s,%s), policy expects (%s,0)" % (context, irq, pre, sub, expected[irq]))
            settings[irq] = pre
        for args, _ in calls(text, "HAL_NVIC_EnableIRQ"):
            require(len(args) == 1 and args[0] in expected, "Unaudited enabled IRQ in " + relative)
            enabled.add(args[0])
        for args, _ in calls(text, "HAL_NVIC_SetPriorityGrouping"):
            require(args == ["NVIC_PRIORITYGROUP_4"], "Non-GROUP4 grouping in " + relative)
            groups.append(relative)
    require(set(settings) == set(expected), "Missing IRQ priority calls: " + str(set(expected) - set(settings)))
    require(enabled == set(expected), "Enabled IRQ set drift: " + str(enabled ^ set(expected)))
    require(groups, "No application GROUP4 configuration found")
    exceptions = {"SVCall_IRQn", "BusFault_IRQn", "DebugMonitor_IRQn", "HardFault_IRQn",
                  "MemoryManagement_IRQn", "NonMaskableInt_IRQn", "UsageFault_IRQn",
                  "SysTick_IRQn", "PendSV_IRQn"}
    ioc_enabled = {key[len("NVIC."):] for key in ioc if re.fullmatch(r"NVIC\.\w+_IRQn", key)
                   and irq_fields(ioc, key[len("NVIC."):])[0] == "true"} - exceptions
    require(ioc_enabled == set(roles), "IOC enabled peripheral set drift: " + str(ioc_enabled ^ set(roles)))
    for irq in roles:
        require(irq_fields(ioc, irq) == ("true", expected[irq], 0), "IOC differs from policy for " + irq)
    for irq in ("TIM4_IRQn", "TIM5_IRQn"):
        require(irq_fields(ioc, irq)[0] == "false", "Traditional control IRQ must stay disabled: " + irq)
    for irq in exceptions - {"SysTick_IRQn", "PendSV_IRQn"}:
        require(irq_fields(ioc, irq)[1:] == (0, 0), "Default exception priority changed: " + irq)

    # The function parameter is traced to actual HAL callers and its saved value.
    require(macros.macro("TICK_INT_PRIORITY") == tick, "HAL TICK_INT_PRIORITY differs from policy")
    require(irq_fields(ioc, "SVCall_IRQn") == ("true", 0, 0), "IOC SVC enable/priority defaults changed")
    require(irq_fields(ioc, "SysTick_IRQn") == ("true", tick, 0), "IOC HAL tick model differs from policy")
    require(irq_fields(ioc, "PendSV_IRQn") == ("true", kernel, 0), "IOC PendSV differs from kernel")
    for irq in ("SVCall_IRQn", "PendSV_IRQn", "SysTick_IRQn"):
        # CubeMX's sixth NVIC field controls ISR generation. FreeRTOS owns these.
        require(ioc["NVIC." + irq].split(r"\:")[5] == "false",
                "CubeMX must not generate the FreeRTOS-owned ISR: " + irq)
    require(ioc.get("VP_SYS_VS_Systick.Mode") == "SysTick", "Existing CubeMX timebase model changed")
    require(not any(key.startswith("NVIC.TIM6") for key in ioc), "Unexpected second CubeMX TIM6 timebase model")
    timer = code["User/Driver/Src/drv_tim.cpp"]
    require(re.search(r"HAL_InitTick\s*\(\s*uint32_t\s+TickPriority\s*\)", timer),
            "Cannot trace TIM6 TickPriority parameter")
    require(re.search(r"uwTickPrio\s*=\s*TickPriority\s*;", timer), "TIM6 must preserve the HAL tick priority")
    require(re.search(r"htim->Instance\s*==\s*TIM6\s*\)\s*\{\s*HAL_IncTick\(\)", timer),
            "TIM6 callback is no longer the isolated HAL tick updater")
    hal = uncomment(read(root / "Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c"))
    rcc = uncomment(read(root / "Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c"))
    require(any(args == ["TICK_INT_PRIORITY"] for args, _ in calls(hal, "HAL_InitTick")),
            "HAL_Init no longer supplies TICK_INT_PRIORITY")
    tick_callers = dict(code)
    tick_callers.update({"HAL/hal.c": hal, "HAL/rcc.c": rcc})
    for relative, text in tick_callers.items():
        for args, _ in calls(text, "HAL_InitTick"):
            require(args in (["TICK_INT_PRIORITY"], ["uwTickPrio"]),
                    "Unaudited HAL tick caller in " + relative + ": " + str(args))
    port = uncomment(read(root / "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/port.c"))
    for exception in ("PENDSV", "SYSTICK"):
        require(re.search(r"#define\s+portNVIC_" + exception + r"_PRI[^\n]*configKERNEL_INTERRUPT_PRIORITY", port),
                "Cannot trace port kernel priority for " + exception)
        require(re.search(r"portNVIC_SYSPRI2_REG\s*\|=\s*portNVIC_" + exception + r"_PRI\s*;", port),
                "Port no longer sets lowest priority for " + exception)

    # Check actual DMA request/stream/arbiter assignments against IOC and IRQ wiring.
    dma_objects = {}
    for relative in ("Core/Src/usart.c", "Core/Src/spi.c"):
        text = code[relative]
        for obj, stream in re.findall(r"\b(hdma_\w+)\.Instance\s*=\s*(\w+)\s*;", text):
            request = re.findall(r"\b" + obj + r"\.Init\.Request\s*=\s*DMA_REQUEST_(\w+)\s*;", text)
            arbiter = re.findall(r"\b" + obj + r"\.Init\.Priority\s*=\s*(\w+)\s*;", text)
            require(len(request) == len(arbiter) == 1, "Cannot parse DMA configuration: " + obj)
            channel = request[0]
            require(channel in DMA_STREAMS and stream == DMA_STREAMS[channel], "DMA request/stream drift: " + obj)
            require(channel not in dma_objects, "Duplicate DMA request: " + channel)
            dma_objects[channel] = obj
            keys = [key[:-len(".Instance")] for key in ioc if re.fullmatch(r"Dma\." + channel + r"\.\d+\.Instance", key)]
            require(len(keys) == 1, "Missing/duplicate IOC DMA channel: " + channel)
            require(ioc[keys[0] + ".Instance"] == stream, "IOC DMA instance drift: " + channel)
            require(ioc.get(keys[0] + ".Priority") == arbiter[0], "IOC DMA arbitration drift: " + channel)
    require(set(dma_objects) == set(DMA_STREAMS), "DMA channel set drift")
    handlers = code["Core/Src/stm32h7xx_it.c"]
    handler_bodies = {name: handlers[start:end] for name, start, end in functions(handlers)}
    for channel, stream in DMA_STREAMS.items():
        body = handler_bodies.get(stream + "_IRQHandler", "")
        require("HAL_DMA_IRQHandler(&" + dma_objects[channel] + ")" in re.sub(r"\s+", "", body),
                "DMA IRQ handler does not dispatch the configured handle: " + channel)
    for uart, obj in UARTS.items():
        body = handler_bodies.get(uart + "_IRQHandler", "")
        require("HAL_UART_IRQHandler(&" + obj + ")" in re.sub(r"\s+", "", body), "UART handler drift: " + uart)

    # Both UART IDLE IRQs and all RX DMA HT/TC IRQs can reach the same FromISR helper.
    routes = {"FDCAN%d_IT0_IRQn" % n for n in (1, 2, 3)} | {"OTG_HS_IRQn"}
    routes |= {uart + "_IRQn" for uart in UARTS}
    routes |= {DMA_STREAMS[channel] + "_IRQn" for channel in DMA_STREAMS if channel.endswith("_RX") and not channel.startswith("SPI")}
    require(all(minimum <= settings[irq] <= kernel for irq in routes), "FromISR route exceeds the syscall ceiling")
    allowed_sites = {"User/Driver/Src/drv_can.cpp": {"CAN_Rx_EnqueueFromISR", "HAL_FDCAN_RxFifo0Callback", "HAL_FDCAN_RxFifo1Callback"},
                     "User/Driver/Src/drv_uart.cpp": {"UART_Rx_EnqueueFromISR", "HAL_UARTEx_RxEventCallback"},
                     "User/Task/Src/freertos_tasks.cpp": {"FreeRTOS_SubmitUSBFromISR"},
                     "USB_DEVICE/App/usbd_cdc_if.c": {"CDC_Receive_HS"}}
    # Presence checks name the reviewed API and owner instead of assuming a
    # total count: some CAN/USB queue helpers currently have no live callers.
    required_sites = {
        ("User/Driver/Src/drv_can.cpp", "CAN_Rx_EnqueueFromISR", "xQueueSendFromISR"),
        ("User/Driver/Src/drv_uart.cpp", "UART_Rx_EnqueueFromISR", "xQueueSendFromISR"),
        ("User/Driver/Src/drv_uart.cpp", "HAL_UARTEx_RxEventCallback", "portYIELD_FROM_ISR"),
        ("User/Task/Src/freertos_tasks.cpp", "FreeRTOS_SubmitUSBFromISR", "xQueueSendFromISR"),
    }
    observed_sites, sites = set(), 0
    for relative, text in code.items():
        spans = list(functions(text))
        for match in re.finditer(r"\b(?:(?:x|v|ux|ul)[A-Z]\w*FromISR|portYIELD_FROM_ISR|portEND_SWITCHING_ISR)\s*\(", text):
            owners = [name for name, start, end in spans if start <= match.start() < end]
            require(len(owners) == 1 and owners[0] in allowed_sites.get(relative, set()),
                    "Unaudited FromISR call in " + relative + ": " + match.group().strip())
            observed_sites.add((relative, owners[0], match.group().split("(", 1)[0].strip()))
            sites += 1
    require(required_sites <= observed_sites,
            "Required FromISR owner/API sites are missing: " + str(sorted(required_sites - observed_sites)))

    # 接收任务已删除：CAN/UART 中断直接投递控制队列，只剩 IMU/控制/后台三个业务任务。
    task_roles = ["IMU", "CONTROL", "TIMER", "BACKGROUND"]
    task_values = [macros.macro("ROBOT_TASK_PRIORITY_" + role) for role in task_roles]
    task_max = macros.macro("configMAX_PRIORITIES")
    require(task_max > task_values[0] and all(a > b for a, b in zip(task_values, task_values[1:])) and task_values[-1] > 0,
            "Task order must be IMU > CONTROL > TIMER > BACKGROUND > idle")
    require(macros.macro("configTIMER_TASK_PRIORITY") == macros.macro("ROBOT_TASK_PRIORITY_TIMER"),
            "Timer service priority differs from the policy")
    task_entries = {"Task_100us_Fast": "IMU", "Task_1ms_Control": "CONTROL", "Task_Background": "BACKGROUND"}
    created = set()
    for relative, text in code.items():
        for args, _ in calls(text, "xTaskCreate"):
            require(len(args) == 6 and args[0] in task_entries, "Unaudited task creation in " + relative)
            role = task_entries[args[0]]
            require(macros.expression(args[4], relative + ": xTaskCreate") == macros.macro("ROBOT_TASK_PRIORITY_" + role),
                    "Task creation priority differs from policy: " + args[0])
            created.add(args[0])
    require(created == set(task_entries), "Expected robot task creation is missing")
    print("PASS: 24 peripheral IRQs + TIM6, 16 FromISR IRQ routes, %d sites, 12 DMA streams/arbiter levels, GROUP4." % sites)
    print("PASS: HAL/IOC tick=%d -> TIM6; runtime SysTick/PendSV=%d; SVC/fault defaults preserved." % (tick, kernel))
    print("PASS: task order %s (Timer below Control); IOC VS_Systick is the deliberate HAL generation model." % task_values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        audit(args.root.resolve())
    except (AuditError, OSError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
