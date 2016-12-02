#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <utility>
#include <unordered_map>
#include <algorithm>

// Helper types for internal data representation
struct Vec3 { double x, y, z; };
struct Vec2 { double x, y; };
struct Vertex {
    bool hasNorm, hasUV;
    Vec3 p;
    Vec3 n;
    Vec2 t;
};

// Write the vertex to the given stream
std::ostream& operator<<(std::ostream& out, const Vertex& v)
{
    out << v.p.x << ", " << v.p.y << ", " << v.p.z;
    if (v.hasNorm)
        out << ", " << v.n.x << ", " << v.n.y << ", " << v.n.z;
    if (v.hasUV)
        out << ", " << v.t.x << ", " << v.t.y;
    return out;
}

// Tokenize the given line into tokens. Returns the number of tokens parsed,
// or -1 if there was an invalid parsing of string to T.
template <typename T>
int tokenize(T out[], const std::string& line, // The line to tokenize
             int maxTokens = 3,          // Max number of tokens parsed
             bool skipFirst = true,      // Skip the first token if true
             char delim = ' ',           // Delimiter that separates tokens
             const T& sentinel = T())    // Sentinel placed for empty lines
{
    std::istringstream ss(line), conv;
    std::string item;

    // Skip the first token in the line if specified
    if (skipFirst)
        std::getline(ss, item, delim);

    // Count up to the max tokens, or until no more tokens
    int count = 0;
    for (; count < maxTokens && std::getline(ss, item, delim); ++count) {
        if (item.length() == 0) {
            // Put sentinel for a blank token
            out[count] = sentinel;
        } else {
            // Try converting to T
            conv.str(item);
            if (!(conv >> out[count]))
                return -1;
            conv.clear();
        }
    }
    return count;
}

// Parses the next vertex attribute by the first two letters.
template <typename V, unsigned int D>
bool parseVertexAttribute(std::istream& in, std::vector<V>& parsedAttribs,
                          char category, char type, std::string& prevLine)
{
    double values[D];
    do {
        // Skip blank lines and comments
        if (prevLine.length() < 2 || prevLine[0] == '#')
            continue;
        // Stop if we've reached a different attribute
        else if (prevLine[0] != category || prevLine[1] != type)
            break;
        // Parse the texture coordinate data
        if (tokenize(values, prevLine) <= 0)
            return false;
        // Write the parsed attribute
        V v; double* vptr = &v.x;
        for (unsigned int i = 0; i < D; ++i)
            *(vptr + i) = values[i];
        parsedAttribs.push_back(v);
    } while (std::getline(in, prevLine));
    return true;
}

// Reads a .obj file and parses the vertex positions, texture coordinates,
// normals, and triangulated faces.
// Returns true on success, otherwise false.
bool objToJs(std::istream& in, std::vector<Vertex>& vertexData, std::vector<unsigned int>& elementData,
             bool disableTexture = false, bool disableNormal = false)
{
    std::string line;

    // Read vertex position data
    std::vector<Vec3> positions;
    if (!parseVertexAttribute<Vec3,3>(in, positions, 'v', ' ', line)) {
        std::cerr << "Malformed vertex position: " << line << std::endl;
        return false;
    } else if (!positions.size()) {
        std::cerr << "Could not parse any vertex positions" << std::endl;
        return false;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after vertex positions" << std::endl;
        return false;
    }

    // Read vertex texture coordinate data
    std::vector<Vec2> texcoords;
    if (!parseVertexAttribute<Vec2,2>(in, texcoords, 'v', 't', line)) {
        std::cerr << "Malformed texture coordinates: " << line << std::endl;
        return false;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after texture coordinates" << std::endl;
        return false;
    }

    // Read vertex normal data
    std::vector<Vec3> normals;
    if (!parseVertexAttribute<Vec3,3>(in, normals, 'v', 'n', line)) {
        std::cerr << "Malformed vertex normals: " << line << std::endl;
        return false;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after vertex normals" << std::endl;
        return false;
    }

    std::string vertices[4];   // Holds 1 triangle or quad
    std::string* triangles[6]; // Holds up to two triangles
    unsigned int locations[3];

    std::unordered_map<std::string, unsigned int> indexCache;

    do {
        // Skip lines that do not specify texture coordinates
        if (line.length() < 2 || line[0] != 'f')
            continue;

        // Parse the texture coordinate data
        int deg = tokenize(vertices, line, 4);
        if (deg != 3 && deg != 4) {
            std::cerr << "All faces must be triangles or quads: " << line << std::endl;
            return false;
        }

        // Get the triangles in this face
        int vertexCount = 3;
        triangles[0] = vertices; triangles[1] = vertices + 1; triangles[2] = vertices + 2;
        if (deg == 4) { // Triangulate quads
            vertexCount = 6;
            triangles[3] = vertices; triangles[4] = vertices + 2; triangles[5] = vertices + 3;
        }

        for (int i = 0; i < vertexCount; ++i) {
            const std::string& v = *(triangles[i]);

            // Search for an already created index for this vertex
            auto it = indexCache.find(v);
            if (it != indexCache.end()) {
                elementData.push_back(it->second);
            } else {
                // Create a new entry in the vertex buffer
                // Get the locations referenced by this vertex
                int attrs = tokenize(locations, v, 3, false, '/');
                if (attrs <= 0) {
                    std::cerr << "Malformed vertex " << v << std::endl;
                    return false;
                }

                // Get the vertex data
                Vec3 pos = positions[locations[0] - 1];
                Vec2 tex;
                if (locations[1] > 0)
                    tex = texcoords[locations[1] - 1];
                Vec3 norm;
                if (locations[2] > 0)
                    norm = normals[locations[2] - 1];

                // Enter and cache the vertex
                elementData.push_back(vertexData.size());
                indexCache[v] = vertexData.size();
                vertexData.push_back({!disableNormal, !disableTexture, pos, norm, tex});
            }
        }
    } while (std::getline(in, line));

    return true;
}

// Sorts the data and remaps the indices such that the data is sorted by
// position Z then X value.
void sortZX(std::vector<Vertex>& data, std::vector<unsigned int>& indices)
{
    // Double comparison
    auto isEqual = [](double a, double b) {
        return std::abs(a - b) < 1e-10;
    };
    unsigned int N = data.size();

    // Build an indexed copy of the data
    typedef std::pair<unsigned int, Vertex> IndexedVert;
    std::vector<IndexedVert> sortedData(N);
    for (unsigned int i = 0; i < N; ++i) {
        sortedData[i] = std::make_pair(i, data[i]);
    }

    // Sort the data by z, x
    std::sort(sortedData.begin(), sortedData.end(), [&isEqual](IndexedVert& v1, IndexedVert& v2) {
        Vec3 &p1 = v1.second.p, &p2 = v2.second.p;
        return p1.z < p2.z || (isEqual(p1.z, p2.z) && p1.x < p2.x);
    });

    // Build the index mapping and copy sorted data
    std::vector<unsigned int> mapping(N);
    for (unsigned int i = 0; i < N; ++i) {
        auto& pair = sortedData[i];
        mapping[pair.first] = i;
        data[i] = pair.second;
    }

    // Write the mapped indices
    for (unsigned int& i : indices) {
        i = mapping[i];
    }
}

// Parses arguments into a dictionary and strips out any '=\d+' suffix into
// the value of the dictionary entry
void parseArguments(std::unordered_map<std::string, int>& parsedArgs,
                    int numArgs, char** args, int offset = 1)
{
    for (int i = offset; i < numArgs; ++i) {
        std::string arg(args[i]);
        // Skip non-arguments
        if (arg[0] != '-' || arg[1] != '-')
            continue;
        // Try parsing the digit
        int digit = 1;
        size_t eqIndex = arg.find_last_of('=');
        if (eqIndex != std::string::npos) {
            digit = std::atoi(arg.substr(eqIndex + 1).c_str());
        }
        // Add the argument to the dictionary
        parsedArgs[arg.substr(0, eqIndex)] = digit;
    }
}

int main(int argc, char* argv[])
{
    std::fstream files[2];
    // Try to get the first two arguments as files
    for (int i = 0; i < 2; ++i) {
        // Try to read the file if it's not an argument
        char* a = argc > i+1 ? argv[i+1] : 0;
        if (a && a[0] != '-' && a[1] != '-') {
            // Read from first, write to second
            files[i].open(a, !i ? std::fstream::in : std::fstream::out);
            if (!files[i].is_open()) {
                std::cerr << "Could not open file " << a << std::endl;
                return -1;
            }
        }
    }
    std::istream& input = files[0].is_open() ? files[0] : std::cin;
    std::ostream& output = files[1].is_open() ? files[1] : std::cout;

    // Parse remaining arguments
    std::unordered_map<std::string, int> parsedArgs;
    parseArguments(parsedArgs, argc, argv);

    // Configure program from arguments
    bool disableTexture = parsedArgs.count("--no-texture");
    bool disableNormal = parsedArgs.count("--no-normal");
    int tabLevel = parsedArgs.count("--indent") ? parsedArgs["--indent"] : 0;
    bool useTabs = parsedArgs.count("--use-tabs");
    int precision = parsedArgs.count("--precision") ? parsedArgs["--precision"] : 5;
    bool doSortZX = parsedArgs.count("--sort-zx");

    // Read and parse the obj file
    std::vector<Vertex> vbo;
    std::vector<unsigned int> ebo;
    if (!objToJs(input, vbo, ebo, disableTexture, disableNormal))
        return -1;

    // Do post processing of results
    if (doSortZX)
        sortZX(vbo, ebo);

    // Configure output
    std::string indent(useTabs ? tabLevel : 4*tabLevel, useTabs ? '\t' : ' ');
    output.precision(precision);

    // Output the vertex buffer array
    output << indent << "// Vertex Buffer Object\n";
    for (auto& v : vbo) {
        output << indent << v << ",\n";
    }
    output << std::endl;
    // Output the element index array
    output << indent << "// Element Index Array\n";
    for (size_t i = 0, N = (size_t)ebo.size(); i < N; ++i) {
        if (i % 3 == 0)
            output << indent;
        output << ebo[i] << ','  << (i % 3 == 2 ? '\n' : ' ');
    }
    output << std::endl;

    // Close resources
    for (auto& file : files) {
        if (file.is_open())
            file.close();
    }
    return 0;
}
