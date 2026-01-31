#!/usr/bin/env python3
# Stolen from https://github.com/enesbcs/rpieasy/blob/aa9e22ef35aab5270f56f322754af41c651c9dfe/lib/lib_syspwm.py#L22
import os

# Copyright 2018 Jeremy Impson <jdimpson@acm.org>

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses>.

# Based on https://github.com/jdimpson/syspwm/

class HPWM(object):
    base_chippath = "/sys/class/pwm/pwmchip"

    def __init__(self,chip_id,pwm):
        success = False
        self.chippath = "{chippath}{chip_id}".format(chip_id=chip_id,chippath=self.base_chippath)
        self.pwm=pwm
        self.enabled = False
        self.dcycle = 0
        self.period = 0
        self.pwmdir="{chippath}/pwm{pwm}".format(chippath=self.chippath,pwm=self.pwm)
        if not self.overlay_loaded() or not self.export_writable():
         return
        else:
         if not self.pwmX_exists():
          self.create_pwmX()
        return

    def __del__(self):
       try:
        if self.enabled:
         self.disable()
       except:
        pass

    def overlay_loaded(self):
        return os.path.isdir(self.chippath)

    def export_writable(self):
        return os.access("{chippath}/export".format(chippath=self.chippath), os.W_OK)

    def pwmX_exists(self):
        return os.path.isdir(self.pwmdir)

    def echo(self,m,fil):
        with open(fil,'w') as f:
            f.write("{tv}\n".format(tv=m))

    def create_pwmX(self):
        pwmexport = "{chippath}/export".format(chippath=self.chippath)
        self.echo(self.pwm,pwmexport)

    def enable(self,disable=False):
        enable = "{pwmdir}/enable".format(pwmdir=self.pwmdir)
        num = 1
        if disable:
            num = 0
            self.enabled = False
        else:
            if self.period<self.dcycle:
             return False # duty cycle never exceed period
            self.enabled = True
        # print(f"PWM: {num} -> {enable}")
        self.echo(num,enable)

    # echo "normal" > polarity
    def polarity(self,normal=True):
        pol = "{pwmdir}/polarity".format(pwmdir=self.pwmdir)
        if normal:
            self.echo("normal",pol)
        else:
            self.echo("inversed",pol)

    def disable(self):
        return self.enable(disable=True)

    def stop(self):
     self.disable()

    def set_frequency(self,hz):
        per = (1 / float(hz))
        per *= 1000    # now in milliseconds
        per *= 1000000 # now in nanoseconds
        self.period = int(per)
        period = "{pwmdir}/period".format(pwmdir=self.pwmdir)
        # print(f"Setting frequency: {hz} Hz -> {self.period} ns -> {period}")
        self.echo(self.period,period)

    def set_duty_prop(self,proportion): # proportion 0.0 .. 100.0
        if proportion>100:
         prop = 100
        elif proportion<0:
         prop = 0
        else:
         prop = proportion
        self.dcycle = int(self.period * float(prop/100))
        duty_cycle = "{pwmdir}/duty_cycle".format(pwmdir=self.pwmdir)
        self.echo(self.dcycle,duty_cycle)

    def set_duty_cycle(self,milliseconds):
        dc = int(milliseconds * 1000000)
        if self.enabled:
         if self.period<dc:
          dc = self.period
        self.dcycle = int(dc)
        duty_cycle = "{pwmdir}/duty_cycle".format(pwmdir=self.pwmdir)
        # print(f"Setting duty cycle: {self.dcycle} ns -> {duty_cycle}")
        self.echo(self.dcycle,duty_cycle)
