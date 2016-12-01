# OBJ to Array Converter
Parses a Wavefront OBJ file and outputs two comma separated lists of numbers.
The first list is a floating-point vertex array that defines one vertex per line.
The second list is an integer index array that defines one triangle per line.

Notes
-----
- Vertex attributes are interleaved
- Quads are triangulated into two triangles in the index array
- Does not support meshes that have faces of degree higher than 4
- Only supports the following attributes: position (v), texture coordinate (vt), normal (vn)
