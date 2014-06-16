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


import argparse
import functools
import os.path
import subprocess
import sys
import tempfile

import jinja2

import ygor


class YgorPlotException(RuntimeError): pass


def terminal(ctx, termtype):
    if termtype == 'svg':
        ctx['ext'] = 'svg'
        return 'set terminal svg size 480, 360 fixed enhanced fname "Sans" fsize 9 rounded dashed\n'
    else:
        raise YgorPlotException('unknown terminal type %r' % termtype)


def output(ctx, name):
    foo, ext = os.path.splitext(name)
    if ext.lstrip('.') == ctx['ext']:
        name = foo
    ctx['out'] = name
    if not os.path.exists(os.path.dirname(name)):
        os.makedirs(os.path.dirname(name))
    return 'set output "{0}.{1}"\n'.format(name, ctx['ext'])


def default_parameter(ctx, name, value):
    if name not in ctx['params']:
        ctx['params'][name] = value


def parameter(ctx, name, value):
    ctx['params'][name] = value


def experiment_base(ctx, exp, trial, name=None):
    exp = ygor.get_experiment(exp)
    exp.load_parameters_from_dict(ctx['params'])
    return exp.get_output_dir(trial, name)


def add_cdf(ctx, datafile, flags=None, title=None):
    ctx['cdfs'].append((datafile, flags, title))


def plot_cdf(ctx, *args):
    tmp = tempfile.NamedTemporaryFile(prefix='ygor-cdf.')
    ctx['tmp'].append(tmp)
    def ygor_args(p, f, t):
        if f is not None:
            return str(f) + '@' + p
        return p
    plots = tuple([ygor_args(p, f, t) for p, f, t in ctx['cdfs']])
    subprocess.check_call(('ygor', 'cdf') + args + plots, stdout=tmp)
    plots = []
    for idx, (plot, flags, title) in enumerate(ctx['cdfs']):
        plots.append('"{name}" using 1:{idx} with linespoints title "{title}"'
                .format(name=tmp.name, idx=idx + 2, title=title))
    return 'plot ' + ', \\\n     '.join(plots) + ';'


def expand_context(context, parameters=None):
    ctx = {}
    ctx['params'] = (parameters or {}).copy()
    ctx['cdfs'] = []
    ctx['tmp'] = []
    context['ctx'] = ctx
    context['parameters'] = {}
    context['terminal'] = functools.partial(terminal, ctx)
    context['output'] = functools.partial(output, ctx)
    context['default_parameter'] = functools.partial(default_parameter, ctx)
    context['parameter'] = functools.partial(parameter, ctx)
    context['experiment_base'] = functools.partial(experiment_base, ctx)
    context['add_cdf'] = functools.partial(add_cdf, ctx)
    context['plot_cdf'] = functools.partial(plot_cdf, ctx)


def plot(name, parameters=None, variables=None, makefile=False):
    loader1 = jinja2.FileSystemLoader(".")
    loader2 = jinja2.FileSystemLoader(os.path.dirname(name))
    loader = jinja2.ChoiceLoader([loader1, loader2])
    env = jinja2.Environment(loader=loader, extensions=['jinja2.ext.do'])
    template = env.get_template(name)
    variables = variables or {}
    context = variables.copy()
    expand_context(context, parameters)
    data = template.render(context)
    if not data.endswith('\n'):
        data += '\n'
    tmp = tempfile.NamedTemporaryFile(prefix=name + '.', dir='.')
    tmp.write(data.encode('utf8'))
    tmp.flush()
    subprocess.check_call(('gnuplot', tmp.name))
    tmp.close()
    for x in context['ctx']['tmp']:
        x.close()
    if makefile:
        target = '{0}.{1}'.format(context['ctx']['out'], context['ctx']['ext'])
        inputs  = []
        inputs += [p.replace('=', '\\=') for p, f, t in context['ctx']['cdfs']]
        if inputs:
            f = open(target + '.ygor.P', 'w')
            f.write(target.replace('=', '\\=') + ': \\\n\t' + ' \\\n\t'.join(inputs) + '\n')
            for i in inputs:
                f.write('{0}:\n'.format(i))
        rules = set()
        if os.path.exists('ygor.P'):
            f = open('ygor.P')
            rules = set(f.read().split('\n'))
            f.close()
        rules.add('-include {0}.ygor.P'.format(target))
        if '' in rules: rules.remove('')
        f = open('ygor.P', 'w')
        f.write('\n'.join(sorted(rules)) + '\n')
        f.flush()
        f.close()


def parse_kv_pairs(kv_pairs):
    d = {}
    for kv in kv_pairs:
        param, value = kv.split('=', 1)
        d[param] = value
    return d


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--param', '--parameter',
                        default=[], dest='parameters', action='append',
                        help='Override parameters of the graph')
    parser.add_argument('-v', '--variable',
                        default=[], dest='variables', action='append',
                        help='Override variables of the graph')
    parser.add_argument('plots', nargs='+', help='A list of gnuplot files')
    args = parser.parse_args(argv)
    parameters = parse_kv_pairs(args.parameters)
    variables = parse_kv_pairs(args.variables)
    for p in args.plots:
        try:
            plot(p, parameters, variables, True)
        except jinja2.exceptions.UndefinedError as e:
            print('{0}: {1}'.format(p, e))
        except subprocess.CalledProcessError as e:
            print('{0}: {1} failed'.format(p, e.cmd[0]))


if __name__ == '__main__':
    main(sys.argv[1:])
