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

import configparser
import argparse
import collections
import errno
import glob
import importlib
import inspect
import itertools
import os.path
import pipes
import select
import shutil
import subprocess
import sys
import tempfile

import paramiko

import ygor


__all__ = ('Experiment', 'Host', 'HostSet', 'Parameter', 'Environment', 'Utility')


class Experiment(object):

    def __init__(self, path):
        self.path = path
        self.output = None

    def get_path(self):
        return self.path

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

    def get_name_for_param(self, p):
        for name in dir(self):
            attr = getattr(self, name)
            if not isinstance(attr, ygor.Parameter):
                continue
            if getattr(self, name) is p:
                return name
        return None

    def get_envvars(self):
        envvars = []
        for name in dir(self):
            attr = getattr(self, name)
            if not isinstance(attr, ygor.Environment):
                continue
            if name.upper() != name:
                raise RuntimeError('Environment %s is not upper case' % name)
            envvars.append(name.lower())
        return sorted(envvars)

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

    def get_output_dir(self, trial, name=None, iteration=None):
        path = self.get_path()
        ps = self.get_parameter_string()
        x = os.path.join(path, ps, trial)
        if name is not None:
            x += '-' + name
        if iteration is not None:
            if iteration == 'auto':
                if not os.path.exists(x):
                    return x
                for idx in itertools.count(1):
                    t = x + '~' + str(idx)
                    if not os.path.exists(t):
                        iteration = idx
                        break
            x += '~' + str(iteration)
        return x

    def load_parameters_from_dict(self, d):
        for param in self.get_parameters():
            old_value = getattr(self, param.upper())
            new_value = d.get(param, old_value)
            if new_value is not old_value:
                new_value = old_value.cast(new_value)
                setattr(self, param.upper(), new_value)

    def load_parameters_from_config(self, config):
        for param in self.get_parameters():
            old_value = getattr(self, param.upper())
            new_value = config.get_parameter(param, old_value)
            if new_value is not old_value:
                new_value = old_value.cast(new_value)
                setattr(self, param.upper(), new_value)

    def load_parameters_from_cmdline(self, cmdline):
        params = set(self.get_parameters())
        for kv in cmdline:
            param, value = kv.split('=', 1)
            if param not in params:
                raise RuntimeError('Command line parameter %s not an '
                                   'experimental parameter' % param)
            new_value = getattr(self, param.upper()).cast(value)
            setattr(self, param.upper(), Parameter(new_value))

    def load_envvars_from_dict(self, d):
        for param in self.get_envvars():
            old_value = getattr(self, param.upper())
            new_value = d.get(param, old_value)
            if new_value is not old_value:
                new_value = old_value.cast(new_value)
                setattr(self, param.upper(), new_value)

    def load_envvars_from_config(self, config):
        for param in self.get_envvars():
            old_value = getattr(self, param.upper())
            new_value = config.get_envvar(param, old_value)
            if new_value is not old_value:
                new_value = old_value.cast(new_value)
                setattr(self, param.upper(), new_value)

    def load_envvars_from_cmdline(self, cmdline):
        params = set(self.get_envvars())
        for kv in cmdline:
            param, value = kv.split('=', 1)
            if param not in params:
                raise RuntimeError('Command line environment variable %s not an '
                                   'experimental environment variable')
            new_value = getattr(self, param.upper()).cast(value)
            setattr(self, param.upper(), Parameter(new_value))

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

    def save_measurement(self, save_as, data):
        print('saving measurement', save_as)
        f = open(os.path.join(self.output, save_as), 'w')
        f.write(data)
        f.flush()
        f.close()


class Configuration(object):

    def __init__(self, exp, config, pcmdline, ecmdline):
        self.cp = configparser.RawConfigParser(allow_no_value=True)
        self.cp.read(config)
        exp.load_parameters_from_config(self)
        exp.load_parameters_from_cmdline(pcmdline)
        exp.load_envvars_from_config(self)
        exp.load_envvars_from_cmdline(ecmdline)
        exp.load_hosts_from_config(self)
        exp.load_hostsets_from_config(self)
        if not self.cp.has_section('parameters'):
            self.cp.add_section('parameters')
        if not self.cp.has_section('envvars'):
            self.cp.add_section('envvars')
        for p in exp.get_parameters():
            v = getattr(exp, p.upper())
            self.cp.set('parameters', p, v)
        for p in exp.get_envvars():
            v = getattr(exp, p.upper())
            self.cp.set('envvars', p, v)

    def get_parameter(self, param, old_value):
        try:
            return old_value.cast(self.cp.get('parameters', param))
        except configparser.NoSectionError as e:
            return old_value
        except configparser.NoOptionError as e:
            return old_value

    def get_envvar(self, param, old_value):
        try:
            return old_value.cast(self.cp.get('envvars', param))
        except configparser.NoSectionError as e:
            return old_value
        except configparser.NoOptionError as e:
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
            ('location', 'username', 'workspace', 'profile'))

    class HostState(object):

        def __init__(self, metadata, command):
            self.location = metadata.location
            self.ssh = paramiko.SSHClient()
            self.ssh.load_system_host_keys()
            self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            self.ssh.connect(metadata.location, username=metadata.username)
            self.transport = self.ssh.get_transport()
            self.channel = self.transport.open_session()
            if metadata.profile:
                cmd  = 'read -n1 && cd %s && source %s && ' % (pipes.quote(metadata.workspace),
                                                   pipes.quote(metadata.profile))
            else:
                cmd  = 'read -n1 && cd %s && ' % pipes.quote(metadata.workspace)
            cmd += command
            self.channel.exec_command(cmd)
            self.out_stdout = ''
            self.out_stderr = ''
            self.exit_status = None

        def recv(self):
            did_something = True
            while did_something:
                did_something = False
                if self.channel.recv_ready():
                    self.out_stdout += self.channel.recv(1024).decode('ascii', 'ignore')
                    did_something = True
                elif self.channel.recv_stderr_ready():
                    self.out_stderr += self.channel.recv_stderr(1024).decode('ascii', 'ignore')
                    did_something = True
                elif self.channel.exit_status_ready():
                    self.exit_status = self.channel.exit_status

        def close(self):
            self.ssh.close()

    @classmethod
    def _output(cls, host, tag, text):
        lines = text.split('\n')
        if lines and not lines[-1]:
            lines.pop()
        for line in lines:
            print('%s:%s: %s' % (tag, host, line))

    @classmethod
    def ssh(cls, hosts, command, status):
        states = [SSH.HostState(h, command) for h in hosts]
        return cls.ssh_wait(states, command, status)

    @classmethod
    def ssh_wait(cls, states, command, status):
        channels = set([])
        fd2chan = {}
        poll = select.poll()
        for s in states:
            fd2chan[s.channel.fileno()] = s
            channels.add(s)
            poll.register(s.channel, select.POLLIN | select.POLLHUP | select.POLLERR)
        for s in states:
            s.channel.send('x')
        while channels:
            for x, ev in poll.poll(1.0):
                x = fd2chan[x]
                x.recv()
                if x.exit_status is not None:
                    x.close()
                    poll.unregister(x.channel)
                    channels.remove(x)
        for host in states:
            cls._output(host.location, 'stdout', host.out_stdout)
            cls._output(host.location, 'stderr', host.out_stderr)
        for host in states:
            if status is not None and host.exit_status != status:
                raise RuntimeError('Host %s failed to execute %s' %
                                   (host.location, command))
        for host in states:
            host.close()

    @classmethod
    def scp(cls, host, username, src, dst):
        ssh = None
        sftp = None
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(host, username=username)
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


class Raw(object):

    def __init__(self, s):
        self.s = s


def quote(s):
    if isinstance(s, Raw):
        return str(s.s)
    return pipes.quote(str(s))


class Host(object):

    def __init__(self, name):
        self.exp = None
        self.name = name
        self.location = None
        self.username = None
        self.workspace = None
        self.profile = ''

    def load_from_config(self, config):
        opts = config.get_host_options(self.name)
        for o in MANDATORY_OPTIONS:
            if o not in list(opts.keys()):
                raise RuntimeError('Host %s missing option %s' % (self.name, o))
        self.location = opts['location']
        self.workspace = opts['workspace']
        if 'username' in opts:
            self.username = opts['username']
        if 'profile' in opts:
            self.profile = opts['profile']

    def run(self, command, status=0):
        command = ' '.join([quote(arg) for arg in command])
        print('run on', self.name + ':', command)
        host = SSH.HostMetaData(location=self.location,
                                username=self.username,
                                workspace=self.workspace,
                                profile=self.profile)
        SSH.ssh([host], command, status)

    def collect(self, save_as, copy_from=None):
        copy_from = copy_from or save_as
        print('collect from', self.name + ':', copy_from, '->', save_as)
        SSH.scp(self.location, self.username,
                os.path.join(self.workspace, copy_from),
                os.path.join(self.exp.output, save_as))


class HostSet(object):

    class Index(object):

        def __init__(self, func):
            self.func = func

    def __init__(self, name):
        self.exp = None
        self.name = name
        self.hosts = []

    def __len__(self):
        return len(self.hosts)

    def load_from_config(self, config):
        defaults = config.get_host_options(self.name)
        if 'number' not in defaults:
            raise RuntimeError('HostSet %s missing option number' % self.name)
        for i in range(int(defaults['number'])):
            name = self.name + ('[%d]' % i)
            opts = defaults.copy()
            opts.update(config.get_host_options(name))
            for o in MANDATORY_OPTIONS:
                if o not in list(opts.keys()):
                    raise RuntimeError('Host %s missing option %s' % (name, o))
            self.hosts.append(SSH.HostMetaData(location=opts['location'],
                                               username=opts.get('username', None),
                                               workspace=opts['workspace'],
                                               profile=opts.get('profile', '')))

    def run(self, command, status=0):
        command = ' '.join([quote(arg) for arg in command])
        print('run on', self.name + ':', command)
        SSH.ssh(self.hosts, command, status)

    def deindex(self, i, arg):
        if isinstance(arg, HostSet.Index):
            return arg.func(i)
        return arg

    def run_many(self, command, status=0, number=None, cond=None):
        if number is None:
            number = len(self.hosts)
        assert self.exp
        def pretty_filter(x):
            if isinstance(x, Parameter):
                name = self.exp.get_name_for_param(x) or '?'
                return 'Parameter({0}={1})'.format(name.lower(), str(x))
            elif isinstance(x, HostSet.Index):
                name = repr(x.func).split()[1]
                return 'HostSet.Index({0})'.format(name)
            else:
                return quote(x)
        pretty = [pretty_filter(a) for a in command]
        print('run on', self.name + ':', ' '.join(pretty))
        states = []
        for idx in range(number):
            if cond is not None and not cond(idx):
                continue
            commandp = [self.deindex(idx, a) for a in command]
            commandp = ' '.join([quote(arg) for arg in commandp])
            h = self.hosts[idx % len(self.hosts)]
            states.append(SSH.HostState(h, commandp))
        SSH.ssh_wait(states, command, status)

    def collect(self, output, source, merge=None, number=None):
        print('collecting from', self.name + ': ->', output)
        tmp = os.path.join(self.exp.output, '.tmp')
        if not os.path.exists(tmp):
            os.mkdir(tmp)
        if number is None:
            number = len(self.hosts)
        dsts = [os.path.join(self.exp.output, output)]
        for idx in range(number):
            host = self.hosts[idx % len(self.hosts)]
            src = os.path.join(host.workspace, self.deindex(idx, source))
            dst = os.path.join(tmp, '{0}-{1}'.format(idx, output))
            SSH.scp(host.location, host.username, src, dst)
            dsts.append(dst)
        if merge is None:
            merge = ['ygor', 'merge', '--output']
        subprocess.check_call(merge + dsts)
        shutil.rmtree(tmp)


class Parameter(object):

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return str(self.value)

    def as_int(self):
        if isinstance(self.value, Parameter):
            value = self.value.value
        else:
            value = self.value
        assert isinstance(value, int) or isinstance(value, int)
        return value

    def cast(self, value):
        if isinstance(value, Parameter):
            value = value.value
        try:
            return Parameter(type(self.value)(value))
        except ValueError as e:
            raise RuntimeError(e)


class Environment(object):

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return str(self.value)

    def as_int(self):
        if isinstance(self.value, Parameter):
            value = self.value.value
        else:
            value = self.value
        assert isinstance(value, int) or isinstance(value, int)
        return value

    def cast(self, value):
        if isinstance(value, Environment):
            value = value.value
        try:
            return Environment(type(self.value)(value))
        except ValueError as e:
            raise RuntimeError(e)


def Utility(original_function):
    original_function.utility = True
    return original_function


def get_experiment(path):
    mod, cls = path.rsplit('.', 1)
    try:
        return getattr(importlib.import_module(mod), cls)(path)
    except ImportError as e:
        raise RuntimeError('Could not import module %s.\n'
                           'Make sure it\'s in your Python path.\nImportError: %s' % (mod, e.message))
    except AttributeError:
        raise RuntimeError('No class %s in module %s.' % (cls, mod))


def run(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--param', '--parameter',
                        default=[], dest='params', action='append',
                        help='Override parameters of the experiment')
    parser.add_argument('-e', '--env',
                        default=[], dest='envs', action='append',
                        help='Override environment variables of the experiment')
    parser.add_argument('--name', default=None,
                        help='The name for this experiment')
    parser.add_argument('--iteration', default=None,
                        help='The iteration of this experiment')
    parser.add_argument('--overwrite', default=False, action='store_true',
                        help='Overwrite previous results')
    parser.add_argument('experiment', help='Class name for the experiment')
    parser.add_argument('configuration', help='Configuration for experiment')
    parser.add_argument('trials', nargs='+', help='A list of trials to run')
    args = parser.parse_args(argv)
    exp = get_experiment(args.experiment)
    cfg = Configuration(exp, args.configuration, args.params, args.envs)
    for trial_name in args.trials:
        if not hasattr(exp, trial_name):
            raise RuntimeError('Experiment has no trial %s' % trial_name)
        path = exp.get_output_dir(trial_name, args.name, args.iteration)
        if os.path.exists(path) and not args.overwrite:
            raise RuntimeError('Experiment already run.  '
                               'To run, erase:\n' + path)
    for trial_name in args.trials:
        path = exp.get_output_dir(trial_name, args.name, args.iteration)
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
    exp = get_experiment(args.experiment)
    exp.get_parameters()
    exp.get_envvars()
    cp = configparser.RawConfigParser(allow_no_value=True)
    cp.add_section('parameters')
    cp.add_section('envvars')
    for p in exp.get_parameters():
        v = getattr(exp, p.upper())
        cp.set('parameters', p, v)
    for p in exp.get_envvars():
        v = getattr(exp, p.upper())
        cp.set('envvars', p, v)
    cp.add_section('hosts')
    for h in exp.get_hosts():
        cp.set('hosts', h + '.location', '<location>')
        cp.set('hosts', h + '.workspace', '<workspace>')
        cp.set('hosts', h + '.profile', '<profile>')
    for h in exp.get_hostsets():
        cp.set('hosts', h + '.number', '1')
        cp.set('hosts', h + '.location', '<default location>')
        cp.set('hosts', h + '.workspace', '<default workspace>')
        cp.set('hosts', h + '.profile', '<default profile>')
        cp.set('hosts', h + '[0].location', '<location>')
        cp.set('hosts', h + '[0].workspace', '<workspace>')
        cp.set('hosts', h + '[0].profile', '<profile>')
    cp.write(sys.stdout)
