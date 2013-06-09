/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include <pygobject.h>

void spice_register_classes (PyObject *d);
void spice_add_constants(PyObject *module, const gchar *strip_prefix);
extern PyMethodDef spice_functions[];

DL_EXPORT(void) initSpiceClientGtk(void)
{
    PyObject *m, *d;

    init_pygobject();

    m = Py_InitModule("SpiceClientGtk", spice_functions);
    if (PyErr_Occurred())
        Py_FatalError("can't init module");

    d = PyModule_GetDict(m);
    if (PyErr_Occurred())
        Py_FatalError("can't get dict");

    spice_register_classes(d);
    spice_add_constants(m, "SPICE_");

    if (PyErr_Occurred()) {
        Py_FatalError("can't initialise module SpiceClientGtk");
    }
}
