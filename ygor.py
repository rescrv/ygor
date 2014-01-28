# Copyright (c) 2014, Robert Escriva
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Ygor nor the names of its contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


from __future__ import absolute_import
from __future__ import print_function
from __future__ import unicode_literals
from __future__ import with_statement

import ConfigParser
import argparse
import collections
import errno
import glob
import importlib
import inspect
import os.path
import pipes
import select
import shutil
import subprocess
import sys
import tempfile

import paramiko

import ygor


__all__ = ('Experiment', 'Host', 'HostSet', 'Parameter', 'Utility')


class Experiment(object):

    def __init__(self):
        self.output = None

    def get_parameters(self):
        params = []
        for name in dir(self):
            attr = getattr(self, name)
            if not isinstance(attr, ygor.Parameter):
                continue
            if name.upper() != name:
                raise RuntimeError('Parameter %s is not upper case' % name)
            params.append(name.lower())
        return sorted(params)

    def get_hosts(self):
        hosts = []
        for name in dir(self):
            attr = getattr(self, name)
            if not isinstance(attr, ygor.Host):
                continue
            if name.upper() != name:
                raise RuntimeError('Host %s is not upper case' % name)
            hosts.append(name.lower())
        return sorted(hosts)

    def get_hostsets(self):
        hostsets = []
        for name in dir(self):
            attr = getattr(self, name)
            if not isinstance(attr, ygor.HostSet):
                continue
            if name.upper() != name:
                raise RuntimeError('HostSet %s is not upper case' % name)
            hostsets.append(name.lower())
        return sorted(hostsets)

    def get_parameter_string(self):
        params = []
        for p in self.get_parameters():
            x = getattr(self, p.upper())
            params.append(p + '=' + str(x))
        return ','.join(params)

    def load_parameters_from_config(self, config):
        for param in self.get_parameters():
            old_value = getattr(self, param.upper())
            new_value = config.get_parameter(param, old_value)
            if old_value != new_value:
                setattr(self, param.upper(), new_value)

    def load_parameters_from_cmdline(self, cmdline):
        params = set(self.get_parameters())
        for kv in cmdline:
            param, value = kv.split('=', 1)
            if param not in params:
                raise RuntimeError('Command line parameter %s not an '
                                   'experimental parameter')
            new_value = getattr(self, param.upper()).cast(value)
            setattr(self, param.upper(), new_value)

    def load_hosts_from_config(self, config):
        for name in self.get_hosts():
            host = getattr(self, name.upper())
            host.load_from_config(config)
            host.exp = self

    def load_hostsets_from_config(self, config):
        for name in self.get_hostsets():
            hostset = getattr(self, name.upper())
            hostset.load_from_config(config)
            hostset.exp = self


class Configuration(object):

    def __init__(self, exp, config, cmdline):
        self.cp = ConfigParser.RawConfigParser(allow_no_value=True)
        self.cp.read(config)
        exp.load_parameters_from_config(self)
        exp.load_parameters_from_cmdline(cmdline)
        exp.load_hosts_from_config(self)
        exp.load_hostsets_from_config(self)
        if not self.cp.has_section('parameters'):
            self.cp.add_section('parameters')
        for p in exp.get_parameters():
            v = getattr(exp, p.upper())
            self.cp.set('parameters', p, v)

    def get_parameter(self, param, old_value):
        try:
            return old_value.cast(self.cp.get('parameters', param))
        except ConfigParser.NoSectionError as e:
            return old_value
        except ConfigParser.NoOptionError as e:
            return old_value

    def get_host_options(self, host):
        return dict([(k[len(host) + 1:], v) for k, v
                     in self.cp.items('hosts')
                     if k.startswith(host + '.')])

    def save(self, path):
        out = open(path, 'w')
        self.cp.write(out)
        out.flush()
        out.close()


class SSH(object):

    HostMetaData = collections.namedtuple('HostMetaData',
            ('location', 'workspace', 'path'))

    class HostState(object):

        def __init__(self, host, workspace, path, command):
            self.location = host
            self.ssh = paramiko.SSHClient()
            self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            self.ssh.connect(host)
            cmd  = 'cd %s && PATH=%s:$PATH ' % (pipes.quote(workspace),
                                                pipes.quote(path))
            cmd += command
            self.stdin, self.stdout, self.stderr = self.ssh.exec_command(cmd)
            self.out_stdout = ''
            self.out_stderr = ''
            self.exit_status = None

        @property
        def fileno(self):
            return self.stdout.channel.fileno

        def recv(self):
            while self.nonblocking_recv():
                pass

        def nonblocking_recv(self):
            if self.stdout.channel.recv_ready():
                self.out_stdout += self.stdout.channel.recv(1024)
                return True
            elif self.stdout.channel.recv_stderr_ready():
                self.out_stderr += self.stdout.channel.recv_stderr(1024)
                return True
            elif self.stdout.channel.exit_status_ready():
                self.exit_status = self.stdout.channel.recv_exit_status()
            return False

    @classmethod
    def _output(cls, host, tag, text):
        lines = text.split('\n')
        if lines and not lines[-1]:
            lines.pop()
        for line in lines:
            print('%s:%s: %s' % (tag, host, line))

    @classmethod
    def ssh(cls, hosts, command, status):
        hosts = [SSH.HostState(h, w, p, command) for h, w, p in hosts]
        while any([h.exit_status is None for h in hosts]):
            for h in hosts:
                h.nonblocking_recv()
            active = [h for h in hosts if h.exit_status is None]
            rl, wl, xl = select.select(active, [], [], 1.0)
            for r in rl:
                r.recv()
        for host in hosts:
            if status is not None and host.exit_status != status:
                raise RuntimeError('Host %s failed to execute %s' %
                                   (host.location, command))
        for host in hosts:
            cls._output(host.location, 'stdout', host.out_stdout)
            cls._output(host.location, 'stderr', host.out_stderr)

    @classmethod
    def scp(cls, host, src, dst):
        ssh = None
        sftp = None
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(host)
            sftp = paramiko.SFTPClient.from_transport(ssh.get_transport())
            sftp.get(src, dst)
        except IOError as e:
            if e.errno == errno.ENOENT:
                raise RuntimeError('Could not copy %s:%s -> %s' %
                                   (host, src, dst))
            raise e
        finally:
            if sftp:
                sftp.close()
            if ssh:
                ssh.close()


MANDATORY_OPTIONS = ('location', 'workspace')


class Host(object):

    def __init__(self, name):
        self.exp = None
        self.name = name
        self.location = None
        self.workspace = None
        self.path = ''

    def load_from_config(self, config):
        opts = config.get_host_options(self.name)
        for o in MANDATORY_OPTIONS:
            if o not in opts.keys():
                raise RuntimeError('Host %s missing option %s' % (self.name, o))
        self.location = opts['location']
        self.workspace = opts['workspace']
        if 'path' in opts:
            self.path = opts['path']

    def run(self, command, status=0):
        command = ' '.join([pipes.quote(str(arg)) for arg in command])
        print('run on', self.name + ':', command)
        host = SSH.HostMetaData(location=self.location,
                                workspace=self.workspace,
                                path=self.path)
        SSH.ssh([host], command, status)

    def collect(self, save_as, copy_from=None):
        copy_from = copy_from or save_as
        print('collect from', self.name + ':', copy_from, '->', save_as)
        SSH.scp(self.location,
                os.path.join(self.workspace, copy_from),
                os.path.join(self.exp.output, save_as))


class HostSet(object):

    def __init__(self, name):
        self.exp = None
        self.name = name
        self.hosts = []

    def load_from_config(self, config):
        defaults = config.get_host_options(self.name)
        if 'number' not in defaults:
            raise RuntimeError('HostSet %s missing option number' % self.name)
        for i in range(int(defaults['number'])):
            name = self.name + ('[%d]' % i)
            opts = defaults.copy()
            opts.update(config.get_host_options(name))
            for o in MANDATORY_OPTIONS:
                if o not in opts.keys():
                    raise RuntimeError('Host %s missing option %s' % (name, o))
            self.hosts.append(SSH.HostMetaData(location=opts['location'],
                                               workspace=opts['workspace'],
                                               path=opts.get('path', '')))


    def run(self, command, status=0):
        command = ' '.join([pipes.quote(str(arg)) for arg in command])
        print('run on', self.name + ':', command)
        SSH.ssh(self.hosts, command, status)

    def collect(self, save_as, copy_from=None):
        copy_from = copy_from or save_as
        print('collect from', self.name + ':', copy_from, '->', save_as)
        for i, host in enumerate(self.hosts):
            SSH.scp(host.location,
                    os.path.join(host.workspace, copy_from),
                    os.path.join(self.exp.output, save_as + ('[%d]' % i)))


class Parameter(object):

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return str(self.value)

    def cast(self, value):
        try:
            return Parameter(type(self.value)(value))
        except ValueError as e:
            raise RuntimeError(e)


def Utility(original_function):
    original_function.utility = True
    return original_function


def get_experiment(path):
    mod, cls = path.rsplit('.', 1)
    try:
        return getattr(importlib.import_module(mod), cls)
    except ImportError:
        raise RuntimeError('Could not find module %s.\n'
                           'Make sure it\'s in your Python path.' % mod)
    except AttributeError:
        raise RuntimeError('No class %s in module %s.' % (cls, mod))


def run(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--param', '--parameter',
                        default=[], dest='params', action='append',
                        help='Override parameters of the experiment')
    parser.add_argument('--name', default=None,
                        help='The name for this experiment')
    parser.add_argument('--overwrite', default=False, action='store_true',
                        help='Overwrite previous results')
    parser.add_argument('experiment', help='Class name for the experiment')
    parser.add_argument('configuration', help='Configuration for experiment parameters')
    parser.add_argument('trials', nargs='+', help='A list of trials to run')
    args = parser.parse_args(argv)
    exp = get_experiment(args.experiment)()
    cfg = Configuration(exp, args.configuration, args.params)
    for trial_name in args.trials:
        if not hasattr(exp, trial_name):
            raise RuntimeError('Experiment has no trial %s' % trial_name)
        ps = exp.get_parameter_string()
        path = os.path.join(args.experiment, ps, trial_name)
        if args.name:
            path += '-' + args.name
        if os.path.exists(path) and not args.overwrite:
            raise RuntimeError('Experiment already run.  '
                               'To run, erase:\n' + path)
        try:
            tmp = tempfile.mkdtemp(prefix='ygor-', dir='.')
            exp.output = tmp
            trial = getattr(exp, trial_name)
            trial()
            cfg.save(os.path.join(tmp, 'config'))
            if not getattr(trial, 'utility', False):
                if os.path.exists(path) and args.overwrite:
                    shutil.rmtree(path)
                shutil.move(tmp, path)
        finally:
            if os.path.exists(tmp):
                shutil.rmtree(tmp)


def configure(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('experiment', help='Class name for the experiment')
    args = parser.parse_args(argv)
    exp = get_experiment(args.experiment)()
    exp.get_parameters()
    cp = ConfigParser.RawConfigParser(allow_no_value=True)
    cp.add_section('parameters')
    for p in exp.get_parameters():
        v = getattr(exp, p.upper())
        cp.set('parameters', p, v)
    cp.add_section('hosts')
    for h in exp.get_hosts():
        cp.set('hosts', h + '.location', '<location>')
        cp.set('hosts', h + '.workspace', '<workspace>')
        cp.set('hosts', h + '.path', '<path>')
    for h in exp.get_hostsets():
        cp.set('hosts', h + '.number', '1')
        cp.set('hosts', h + '.location', '<default location>')
        cp.set('hosts', h + '.workspace', '<default workspace>')
        cp.set('hosts', h + '.path', '<default path>')
        cp.set('hosts', h + '[0].location', '<location>')
        cp.set('hosts', h + '[0].workspace', '<workspace>')
        cp.set('hosts', h + '[0].path', '<path>')
    cp.write(sys.stdout)


def main(argv):
    commands = {'run': run,
                'configure': configure}
    if len(argv) == 0 or argv[0] not in commands:
        print('usage: ygor.py command [arguments [arguments ...]]', file=sys.stderr)
        return 1
    return commands[argv[0]](argv[1:])


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except RuntimeError as e:
        print(e, file=sys.stderr)
        sys.exit(1)
