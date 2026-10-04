#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import random
import shlex
import shutil
import signal
import subprocess
import sys
import time


RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'http://127.0.0.1:8080/api/v1/maps/map1',
    'http://127.0.0.1:8080/api/v1/maps',
]

SHOOT_COUNT = 1000
COOLDOWN = 0.001
PERF_FREQUENCY = 997

SCRIPT_DIR = Path(__file__).resolve().parent
FLAMEGRAPH_DIR = SCRIPT_DIR / 'FlameGraph'
PERF_DATA_TMP = Path('/tmp') / 'game_server.perf.data'
PERF_DATA = SCRIPT_DIR / 'perf.data'
GRAPH_SVG = SCRIPT_DIR / 'graph.svg'
PERF_LOG = SCRIPT_DIR / 'perf.log'

def parse_args():
    parser = argparse.ArgumentParser(
        description='Start server, profile it with perf, send requests and build flame graph.'
    )

    parser.add_argument(
        'server',
        nargs='+',
        help='Server command and its arguments'
    )

    args = parser.parse_args()

    command = ' '.join(args.server)
    server_command = shlex.split(command)

    if not server_command:
        parser.error('Server command must not be empty')

    return server_command


def require_file(path: Path):
    if not path.is_file():
        raise FileNotFoundError(f'Required file not found: {path}')


def check_environment(args):
    if os.name != 'posix':
        raise RuntimeError(
            'This script requires Linux/WSL: Linux perf cannot profile a Windows process.'
        )

    if shutil.which('perf') is None:
        raise RuntimeError(
            'perf was not found. Install it with:\n'
            'sudo apt install linux-tools-common linux-tools-$(uname -r)'
        )

    if args.server[0].lower().endswith('.exe'):
        raise RuntimeError(
            'Windows .exe cannot be profiled with Linux perf.\n'
            'Build and pass the Linux ELF executable, for example:\n'
            './build/bin/game_server'
        )


def stop_perf(process: subprocess.Popen, timeout: float = 20.0):
    if process is None or process.poll() is not None:
        return

    print('Stopping perf record...')

    process.send_signal(signal.SIGINT)

    try:
        return_code = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise RuntimeError(
            'perf record did not stop gracefully; perf.data may be corrupted'
        )

    if return_code not in (0, 130, -signal.SIGINT):
        raise RuntimeError(
            f'perf record finished with unexpected exit code: {return_code}. '
            f'See {PERF_LOG}'
        )


def stop_server(process: subprocess.Popen, timeout: float = 10.0):
    if process is None or process.poll() is not None:
        return

    print('Stopping server...')

    process.terminate()

    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        print('Server did not stop in time, killing it...', file=sys.stderr)
        process.kill()
        process.wait()


def wait_server_start(server: subprocess.Popen, timeout: float = 5.0):
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        exit_code = server.poll()

        if exit_code is not None:
            raise RuntimeError(
                f'Server exited before profiling started, exit code: {exit_code}'
            )

        time.sleep(0.1)


def shoot(url: str):
    result = subprocess.run(
        [
            'curl',
            '-sS',
            '--max-time', '5',
            '--output', '/dev/null',
            '--write-out', '%{http_code}',
            url,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )

    if result.returncode != 0:
        print(
            f'Warning: curl failed for {url}, exit code {result.returncode}',
            file=sys.stderr,
        )
        return

    if result.stdout.strip() != '200':
        print(
            f'Warning: {url} returned HTTP {result.stdout.strip()}',
            file=sys.stderr,
        )


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])

        if COOLDOWN > 0:
            time.sleep(COOLDOWN)

    print('Shooting complete')


def build_flamegraph():
    stackcollapse = FLAMEGRAPH_DIR / 'stackcollapse-perf.pl'
    flamegraph = FLAMEGRAPH_DIR / 'flamegraph.pl'

    require_file(stackcollapse)
    require_file(flamegraph)

    with GRAPH_SVG.open('wb') as graph_file:
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', str(PERF_DATA_TMP)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        stack_collapse = subprocess.Popen(
            ['perl', str(stackcollapse)],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        perf_script.stdout.close()

        flame_graph = subprocess.Popen(
            ['perl', str(flamegraph)],
            stdin=stack_collapse.stdout,
            stdout=graph_file,
            stderr=subprocess.PIPE,
        )

        stack_collapse.stdout.close()

        flamegraph_stderr = flame_graph.communicate()[1]
        stackcollapse_stderr = stack_collapse.communicate()[1]
        perfscript_stderr = perf_script.communicate()[1]

    if perf_script.returncode != 0:
        raise RuntimeError(
            'perf script failed:\n'
            + perfscript_stderr.decode(errors='replace')
        )

    if stack_collapse.returncode != 0:
        raise RuntimeError(
            'stackcollapse-perf.pl failed:\n'
            + stackcollapse_stderr.decode(errors='replace')
        )

    if flame_graph.returncode != 0:
        raise RuntimeError(
            'flamegraph.pl failed:\n'
            + flamegraph_stderr.decode(errors='replace')
        )

    if not GRAPH_SVG.is_file() or GRAPH_SVG.stat().st_size == 0:
        raise RuntimeError('graph.svg was not created or is empty')

    svg = GRAPH_SVG.read_text(encoding='utf-8', errors='ignore')

    if 'RequestHandler' not in svg:
        raise RuntimeError(
            'graph.svg does not contain RequestHandler.\n'
            'Check raw stacks with:\n'
            '  perf script -i perf.data | grep -E "RequestHandler|request_handler"\n'
            'Likely causes: too few samples, inlining, or handler methods were not hit.'
        )

    print(f'Flame graph created: {GRAPH_SVG}')


def main():
    args = parse_args()
    check_environment(args)

    stackcollapse = FLAMEGRAPH_DIR / 'stackcollapse-perf.pl'
    flamegraph = FLAMEGRAPH_DIR / 'flamegraph.pl'

    require_file(stackcollapse)
    require_file(flamegraph)

    PERF_DATA.unlink(missing_ok=True)
    GRAPH_SVG.unlink(missing_ok=True)
    PERF_LOG.unlink(missing_ok=True)

    server = None
    perf = None
    perf_log = None

    try:
        print('Starting server:')
        print(' ', shlex.join(args.server))

        server = subprocess.Popen(
            args.server,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        wait_server_start(server)

        print(f'Server PID: {server.pid}')
        print('Starting perf record...')

        perf_log = PERF_LOG.open('wb')

        perf = subprocess.Popen(
            [
                'perf',
                'record',
                '-e', 'cpu-clock',
                '-F', str(PERF_FREQUENCY),
                '-g',
                '--call-graph', 'dwarf',
                '-p', str(server.pid),
                '-o', str(PERF_DATA_TMP),
            ],
            stdout=subprocess.DEVNULL,
            stderr=perf_log,
        )

        time.sleep(0.5)

        if perf.poll() is not None:
            perf_log.close()
            perf_log = None

            details = PERF_LOG.read_text(encoding='utf-8', errors='replace')
            raise RuntimeError(
                'perf record exited unexpectedly:\n'
                + details
            )

        make_shots()

    finally:
        if perf is not None:
            stop_perf(perf)

        if perf_log is not None and not perf_log.closed:
            perf_log.close()

        if server is not None:
            stop_server(server)

    if not PERF_DATA_TMP.is_file() or PERF_DATA_TMP.stat().st_size == 0:
        details = ''

        if PERF_LOG.is_file():
            details = PERF_LOG.read_text(encoding='utf-8', errors='replace')

        raise RuntimeError(
            'perf.data was not created or is empty.\n'
            f'perf log:\n{details}'
        )

    shutil.copy2(PERF_DATA_TMP, PERF_DATA)

    print(f'perf.data created: {PERF_DATA}')

    build_flamegraph()
    print('Job done')


if __name__ == '__main__':
    main()
