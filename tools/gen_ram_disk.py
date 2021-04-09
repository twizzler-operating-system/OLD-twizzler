#!/usr/bin/env python

import os

import sys

build_dir = sys.argv[2]
hier_cmd = sys.argv[3]
file2obj_cmd = sys.argv[4]
objstat_cmd = sys.argv[5]
append_cmd = sys.argv[6]

class namespace:
    def __init__(self):
        self.namelist = []
    def add_name(self, name, fullpath):
        self.namelist.append( (name, fullpath, None, None) )
    def add_namespace(self, name, ns):
        self.namelist.append( (name, None, ns, None) )
    def add_symlink(self, name, fullpath, sym):
        self.namelist.append( (name, fullpath, None, sym) )
    def __str__(self):
        return str(self.namelist)
    def __repr__(self):
        return str(self.namelist)

stack = []

def process(sysroot):
    global stack
    global namespaces
    for f in os.listdir(sysroot):
        #print(f)
        fullpath = sysroot + "/" + f
        if os.path.islink(fullpath):
            stack[-1].add_symlink(f, fullpath, os.readlink(fullpath))
        elif os.path.isfile(fullpath):
            #print("  file")
            stack[-1].add_name(f, fullpath)
        else:
            #print("  dir")
            n = namespace()
            stack[-1].add_namespace(f, n)
            stack.append(n)
            process(fullpath)
            #namespaces.append(stack[-1])
            stack = stack[0:-1]


import subprocess
def gen_nss(root, ns_name, parent_id, space=0):
    global build_dir
    global hier_cmd
    global file2obj_cmd
    global objstat_cmd
    global append_cmd
    ns_file = build_dir + "/namespace_output" + "/" + ns_name
    objroot_dir = build_dir + "/object_output"
    ns_objname = objroot_dir + "/" + "__nobj_" + ns_name
    f2o = subprocess.run([file2obj_cmd, "-i", "/dev/null", "-o", ns_objname, "-p", "RXW"])
    obst = subprocess.Popen([objstat_cmd, "-i", ns_objname], stdout=subprocess.PIPE)
    ns_objid = obst.communicate()[0].decode().strip()
    obst.wait()
    if parent_id == None:
        parent_id = ns_objid

    #print(ns_file)
    with open(ns_file, "w") as out:
        p = subprocess.Popen([hier_cmd], stdin=subprocess.PIPE, stdout=out)
        #print(ns_name)
        
        lines = []
        lines.append("n " + ns_objid + " .")
        lines.append("n " + parent_id + " ..")
        for (name, fullpath, ns, sym) in root.namelist:
            #for i in range(0, space):
            #    print(" ", end = "")
            #print(name + " == " + str(objid))
            objid = None
            ty = None
            if sym:
                objid = sym
                ty = "s"
                pass
            else:
                if ns:
                    ty = "d"
                    objid = gen_nss(ns, ns_name + "_" + name, ns_objid, space+2)
                else:
                    ty = "r"
                    objname = objroot_dir + "/" + ns_name.replace("__ns_", "__obj_") + "_" + name
                    f2o = subprocess.run([file2obj_cmd, "-i", fullpath, "-o", objname, "-p", "RXH"])
                    obst = subprocess.Popen([objstat_cmd, "-i", objname], stdout=subprocess.PIPE)
                    objid = obst.communicate()[0].decode().strip()
                    obst.wait()
                    os.link(objname, objroot_dir + "/" + objid)
            line = ty + " " + objid + " " + name
            #print(line)
            lines.append(line)

        p.communicate(input="\n".join(lines).encode())
        p.stdin.close()
        p.wait()

    with open(ns_file, "r") as out:
        subprocess.run([append_cmd, ns_objname], stdin=out)

    os.link(ns_objname, objroot_dir + "/" + ns_objid)
    #print("GENERATED " + objname + "for " + ns_name + ":: " + objid)
    return ns_objid

    #return "0000000000000000:0000000000000000"

stack.append(namespace())
process(sys.argv[1])
root = stack[0]
#namespaces.append(stack[-1])
#stack = stack[0:-1]

try:
    os.mkdir(build_dir + "/namespace_output")
except OSError as error:
    pass
try:
    os.mkdir(build_dir + "/object_output")
except OSError as error:
    pass
for i in os.listdir(build_dir + "/object_output"):
    os.unlink(build_dir + "/object_output/" + i)
gen_nss(root, "__ns_root", None)

# TODO: dont hardcode?
obst = subprocess.Popen([objstat_cmd, "-i", build_dir + "/object_output/__obj_root_usr_bin_init_bootstrap"], stdout=subprocess.PIPE)
init_objid = obst.communicate()[0].decode().strip()
obst.wait()

obst = subprocess.Popen([objstat_cmd, "-i", build_dir + "/object_output/__nobj___ns_root"], stdout=subprocess.PIPE)
name_objid = obst.communicate()[0].decode().strip()
obst.wait()

with open(build_dir + "/object_output/kc", "w") as kcf:
    kcf.write("name=" + name_objid + "\n")
    kcf.write("init=" + init_objid + "\n")

objs = []
for i in os.listdir(build_dir + "/object_output"):
    objs.append(i)
subprocess.run(["tar", "cf", build_dir + "/ramdisk.tar", "-C", build_dir + "/object_output",
    "--exclude", "__*", *objs])


