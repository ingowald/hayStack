# HayStack

# Building and Running

### linux package dependencies

    sudo apt install libtbb-dev cmake-curses-gui libglfw3-dev build-essential libopenmpi-dev libucx-dev libpmix-dev libgtk-3-dev

### MPI

To build *haystack* with MPI support, make sure to use `-DHS_MPI=ON` when configuring the project.

# Command Line Options

Though haystack has an interactive viewer it is still largely driven
my command line parameters. In no particular order:

### Camera related:

- `--camera pv py pz cx cv cz ux uy uz`: specifies position
  (px,py,pz), center of what the camera is aiming at (cx, cy, cz) and
  upvector (ux,uy,uz) of the camera
- `-fovy deg`: camera field of view, in y direction, in degrees. Also sets camera type to perspective (default)
- `-height float`: set camera to orthographic mode, and set height of image plane to this value
- `--camera-near-clip float` : offsets all primary rays by this
  distance from their camera origin, allowing for some near-clipping
  plane like behavior.

### Lights related:

- `--dir-light  dx dy dz  rx ry rz`: direction (d) and radiance (r) of a
  directional ("distant") light source
- `--quad-light cx cy cz  e0x e0y e0z e1x e1y e1z  Ex Ey Ez`: a directed
  (one-sided) quadrangular light source, specified through corner (c),
  and its two edge vectors (e0 and e1), as well as emission (E)
- `--quad-light-centered  cx cy cz  e0x e0y e0z  e1x e1y e1z  Ex Ey Ez`:
  same as `--quad-light` except c specifies the *center* of the
  light source
- `--lights miniFileWithLights.mini`: given a scene of light sources
  in `miniScene` format, this loads all the lights sources from this
  model and makes sure they are on all ranks. This will *not* load the
  geometry from this model, only the light sources - but it will
  replicate them across all ranks.
- `--env-map miniWithEnvMap.mini`: same as `--lights`, but
  specifically for an environment map. will at some point get
  superseded by --lights.

### Multi-GPU and/or Multi-Node Parallel Rendering Related:

- `-ndg int`: number of parallel 'data groups' (ie, 'partitions') to
  partition the input content(s) over. Each resource specifier (see
  below) can generate one or more 'pieces' of data; haystack will then
  partition these into the requested number of data groups. It is THIS
  value that specified the number of data parallel partitions for data
  parallel rendering. If intending to render 8-wide data parallel (8
  mpi ranks, or 8 GPUs in a single-process/multi-GPU manner), one has
  to be specify `-ndg 8`.
- `-dp` request data parallel rendering. input data gets partitioned into
  'ndg' partitions, and every set of ndg machines gets one piece of data each
- `-dr` request data *replicated* rendering; all ranks will get all content.

### Unordered:

- `-o filename.png`: file to render to, in offline mode
- `-spp int`: number of samples/paths per pixel
- `--[no-]denoise`: enable/disable denoising
- `--measure`: render for given number of frames/seconds, and print time per frame
- `--bg-color r g b a`: specify (constant) background color
- `--ambient float`: ambient illumination

# Loadable Content Specifiers

Every command line argument passed to haystack that is not in itself a
`-arg` or `--argument` style paramter will be interpreted as a
"content specifier" that specifies some kind of renderable content
that haystack is to render. This is typically a filename, optionally
with some 'type specifier' that specifies what kind of loader to use
for that, plus optionally some arguments for this particular
loader. In some cases the specific content can also be procedural, not
a file, but otherwise the same logic applies.

## Loadable Resource Descriptors: 

Any content specifier (in haystack also called a 'data URL') looks
like this: `[contentType://][<N>@]contentDescriptor[:arg=value]*`

In this, 

- `contentDescriptor` is typically a filename, such as `truck.mini` or
  `~/models/truck.obj`. In some cases it can also be a prodcedural
  content generator.
- `contentType://` specifies the type of content loader to use for
  this content descriptor/content file. E.g., `obj://` would request an
  obj loader, `spheres://` describes a flexible type of binary files
  with spheres in them, etc. A list of supported content specifiers is
  given below. `contentType://` is optional if the type can be inferred
  from the file ame extension, but is required if either `N@` or any
  parameters are being used even if type could otherwise be derived from
  file name.

- `<N>@` requests that haystack partition this particular content/file
  on-the-fly while loading. I.e., `ts.tri://8@triangles.tri` would
  request to treat the file `triangles.tri` as if it was 8 separate
  files with one-eigth of the triangles. Different content loaders may
  interpret differently what 'pre-partitioning' means, some may not
  support it at all.
  
- `:arg=value` pairs. Any number of `:arg0=value0:arg1=value0:...` can
  be appended to any content descriptor, from where the respective
  loader can read them when it starts to read that content. What
  arguments a loader understands - and what format it expects the
  values to be in - depends on the loader.
  
## Currently Supported Resource specifiers

Haystack currently supports the following 'file format'/content descriptors:

### `ts.tri://`: Tim-Sandstrom binary triangles file format. 

Content descriptor is a (binary) file made up of floats that form
triangles. Each triangle is three vertices, where each vertex is made
up of 'stride' floats, the first three of which are x,y,z position,
and others can be used for color mapping. Supported parameters:
  
- `begin=int64`: first triangle to read from this file. first 'begin' tris will simply get ignored.
- `count=int64`: number of triangles to read from this, starting at begin.
- `stride=int`: number of floats per vertex. first three are always x,y,z.
- `sidx=int`: index in per-vertex floats to use for as a scalar for
  scalar-to-color mapping. Eg, `:stride=4:sidx=3` would mean that each
  vertex has four floats, three for position, and fourth (sidx=3) is
  mapped to a color. if not specified no scalar to color mapping
  occurs.
- 'cidx=int': index/offset for an explicit rgb color per vertex. Eg
  `stride=6,cidx=3` would specify that each vertex has 6 floats, three
  for position, and a rgb color value at offset 3.
- any color-mapper specific parameters see below
- any anari material specific parameters see below

### `spheres://`: Binary spheres file format. 

Content descriptor is a (binary) file made up of floats that form
spheres. Each sphere is made up of 'stride' floats, the first three of which are x,y,z position,
and others can be used for color mapping. Supported parameters:
  
- `begin=int64`: first triangle to read from this file. first 'begin' tris will simply get ignored.
- `count=int64`: number of triangles to read from this, starting at begin.
- `stride=int`: number of floats per vertex. first three are always x,y,z.
- `sidx=int`: index in per-vertex floats to use for as a scalar for
  scalar-to-color mapping. Eg, `:stride=4:sidx=3` would mean that each
  vertex has four floats, three for position, and fourth (sidx=3) is
  mapped to a color. if not specified no scalar to color mapping
  occurs.
- 'cidx=int': index/offset for an explicit rgb color per vertex. Eg
  `stride=6,cidx=3` would specify that each vertex has 6 floats, three
  for position, and a rgb color value at offset 3.
- 'ridx=int': index of which float specifies a per-particle radius. If
  not specifies we use the 'radius' value.
- `radius=float`: a global radius to use for each sphere, when ridx is not set.
- any color-mapper specific parameters see below
- any anari material specific parameters see below

Examples:
- `./hsViewer
  spheres://~/models/0004004.vx.xyzf.flt:stride=4:radius=.001:baseColor=.3,.7,.3`:
  A file of spheres, with four floats per sphere, rendering with a
  fixed greenish color.
- `./hsViewer
  spheres://~/models/0004004.vx.xyzf.flt:stride=4:radius=.001:sidx=3:map=-20,80:map-colors=<filename>`:
  A file of spheres, with four floats per sphere; fourth component (index=3) is used for color mapping, mapping from [-20,80] to an ascii color map specified in given file name.

### Anari Material related arguments

Not all, but some resource speficiers allow specifying and
`ANARIMaterial` to be used for the specifying geometry(ies) that this
loader produces. If so, the following arguments are recognized:

- `:baseColor=r,g,b`: specifies base color, default is 1,1,1
- `:emissive=r,g,b`: specifies emissive component of material, default is 0,0,0
- `:roughness=<float>`: roughness, usually around 0.1..0.2 for some sort of glossy lobe
- `:metallic=<float>`: how metallic the material is, in [0,1]
- `:opacity=<float>`: opacity, in [0,1].

Note that some resource loaders may still set other values that
supersede these parameters. For example a `ts.tri` loader may allow to
set the baseColor, but may also set some color-mapping, which
supersede the baseColor in rendering.

### Color-mapper related arguemtns

Not all, but some resource specifiers allow to specify on-the-fly
scalar-to-color mapping (ie, color mapping that happens during load
time, not a transfer function applied to a volume). If so, the
following arguments are recognized:

- `:map=lo,hi` specified input domain for the color mapping, i.e., any
  scalar at or below 'lo' will be mapped to the lower end of the color
  range, and any scalar at or above hi will be mapped to the end of
  the color range. Eg, assuming a 'cold to hot' color map was used
  (see below), and a `:map=-20,80` was specified as input domain, any
  scalar <= -20 would appear 'cold', andy >= 80 would be 'hot' and any
  in-between would fall somewhere in-between.
- `:map-colors=<filename>`: points to a file name with colors to be
  used for color mapping. file is expected to be in ASCII, with four
  floats (r,g,b,a) per line. 'a' is currently ignored.
- `:map-index=int` refers to a set of built-in color maps such as
  'paraview cool/warm', 'covise', 'virdis', etc. map index of -1 means 
  'temperature-to-rgb'



# Example Data Sets and How to Run Them

## Colored Iso-surface of Mars Lander 

![](jpg/lander-iso.jpg)

- download from google drive using this link https://drive.google.com/file/d/1E7QEQ1H-JjPAIjCnCryv2yAqQ2egMiKq/view?usp=sharing
- unpack `tar xavf lander-iso_vortMag_2.4_mapped_w.tgz`
- load `.tstri1` files with haystack:

```
hsOffline lander-iso_vortMag_2.4_mapped_w/*tstri1 \
	-os 1600 1200
	-o lander-iso_vortMag_2.4_mapped_w.png
```
File format for this data: `.tstri1` is 
- all binary file, one triangle after another
- each triangle is three vertices, no connectivity
- each vertex is three floats position, plus one float scalar
 (scalar to color mapping currently happens in loader, tstri1 files
  do not specify how this mapping happens)

## Quick-Tests, for raw (structured) and mini (triangle meshes) files:

![](jpg/collage-quicktests.jpg)

``` bash
./hsViewerQT ~/mini/ls.mini 

./hsViewerQT ~/mini/ls.mini `cat data/ls.cam`

./hsViewerQT ~/mini/rungholt.mini `cat data/rungholt.cam`

./hsViewerQT raw:///cluster/kingsnake_1024x1024x795_uint8.raw:format=uint8:dims=1024,1024,795 `cat data/kingsnake.cam` -xf data/kingsnake.xf
```



