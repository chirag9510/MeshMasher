# MeshMasher
MeshMasher is a file generation tool that takes in multiple 3D model files which then combines and writes out formatted mesh data into custom files for optimized OpenGL rendering.

## Advantages
There are several key advantages that come with the output generated by this tool when integrated into your OpenGL renderer :
* **Reduce driver overhead with a single indirect draw call** \
  This formatted data is intended to be drawn from a single render call with glMultiDrawElements() based on the AZDO principle using indirect draw buffer (GL_DRAW_INDIRECT_BUFFER), reducing driver overhead when compared with multiple render calls with direct draw.
  Vertex and index buffer data is all laid out in a single sequence interleaved format which will only utilize a single vertex array, vertex buffer and element buffer further reducing overhead generated by switching to multiple vertex arrays and buffers.
* **Eliminate load times with MultiThreaded asynchronous buffer data mapping** \
Putting different types of data in different files allows for loading and mapping of this data into the buffer using multiple threads asynchronously which can drastically reduce load times.
* **Mesh data optimization provided by the meshoptimizer library** \
The efficiency of the various stages of the GPU pipeline that have to process vertex and index data when a GPU renders triangle meshes depends on the data you feed to them. This library provides algorithms to help optimize meshes for these stages and algorithms to reduce the mesh complexity and storage overhead. \
Checkout the library for the full reference.
* **Organize mesh data based on appropriate rendering order** 
Transparent material based meshes are written at the end so they can be drawn last as they should.

## Build
You can download the compiled executable and run the program immediately but if you do compile the program yourself, copy and paste the contents of the **bin_cpy** folder into the executable directory. \
**contents.txt, input and output** folders  MUST be present with the executable.

## Usage
First, copy all the model files that need to be processed in the input folder present in the executable directory. \
Then open **contents.txt** and write full filenames (eg. Duck.gltf) of all the models that need to be processed in a list format. \
I highly recommend you checkout the **contents.txt** file and launch the application to see how it all works with the sample models provided in the **input** folder. \

You can either launch the application with the default settings by directly clicking on the executable or you can launch it with custom settings with these command line arguments:
```
# MeshMasher.exe -wt <num worker threads> -ptv <bool 0/1> -mo <bool 0/1>
# -wt = number of worker threads to be used for mesh data processing
# -ptv = set assimp aiProcess_PreTransformVertices flag 
# -mo = use meshoptimizer library on mesh data
# default settings
MeshMasher.exe -wt 2 -ptv 1 -mo 1
```
## Ouput generated
MeshMasher writes different types of data into different files with the intention of letting the geometry loader, that will map data into buffers, being able to do this with multiple threads asynchronously. 
* **.ldr** = loader file containing info required for indirect drawing such as baseVertex, firstIndex, index count, baseInstance etc. 
* **.vbf** = vertex buffer data file containing interleaved vertex data in position/texcoord/normals format. 
* **.ebf** = elements buffer data file containing GL_UNSIGNED_INT format indices for GL_TRIANGLES draw. 
* **.mtr** = material data file. 
* **.txr** = texture data file containing names and characterstics of texture files and used for identification of data in .rgb file. 
* **.rgb** = GL_RGB internal format data file containing raw image data used in conjunction with .txr file for identification. 

These files can be found in the output folder present in the executable folder which can then be tested using the MMViewer application.

## MMViewer
[MMViewer](https://github.com/chirag9510/MMViewer) is an application developed for the sole purpose of testing the output generated by MeshMasher. Executable is released as zip alongside MeshMasher. \
Move the output files generated by the MeshMasher into the **input** folder present in the MMViewer executable directory and just launch. \
 
Here it is rendering each individual model with their multiple instances using a single indirect draw rendering call with glMultiDrawElements():
![mmviewer](https://github.com/chirag9510/MeshMasher/assets/78268919/b90dbba6-45ad-4dca-b24e-1195473476a2)

## Libraries and References
[assimp](https://github.com/assimp/assimp) \
[meshoptimizer](https://github.com/zeux/meshoptimizer) \
[stb_image](https://github.com/nothings/stb) \
[SDL2](https://github.com/libsdl-org/SDL) \
[polyhaven (chest model)](https://polyhaven.com) \
[glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets)
