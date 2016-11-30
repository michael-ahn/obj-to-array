#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

// Helper types for internal data representation
struct Vec3 { double x, y, z; };

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

// Adds the data at the location to the given buffer
bool addDataToBuffer(std::vector<double>& buffer, unsigned int location,
                     const std::vector<Vec3>& refData, bool disable = false,
                     bool is2D = false)
{
    if (location > 0 && !disable) {
        if (location > refData.size())
            return false;
        const Vec3& v = refData[location - 1];
        buffer.push_back(v.x); buffer.push_back(v.y);
        if (!is2D)
            buffer.push_back(v.z);
    }
    return true;
}

// Parses the next vertex attribute by the first two letters.
bool parseVertexAttribute(std::istream& in, std::vector<Vec3>& parsedAttribs,
                          char category, char type, std::string& prevLine)
{
    double values[3];
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
        parsedAttribs.push_back({values[0], values[1], values[2]});
    } while (std::getline(in, prevLine));
    return true;
}

// Reads a .obj file and parses the vertex positions, texture coordinates,
// normals, and triangulated faces.
// Returns true on success, otherwise false.
int objToJs(std::istream& in, std::vector<double>& vertexData, std::vector<double>& elementData,
             bool disableTexture = false, bool disableNormal = false)
{
    std::string line;

    // Read vertex position data
    std::vector<Vec3> positions;
    if (!parseVertexAttribute(in, positions, 'v', ' ', line)) {
        std::cerr << "Malformed vertex position: " << line << std::endl;
        return -1;
    } else if (!positions.size()) {
        std::cerr << "Could not parse any vertex positions" << std::endl;
        return -1;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after vertex positions" << std::endl;
        return -1;
    }

    // Read vertex texture coordinate data
    std::vector<Vec3> texcoords;
    if (!parseVertexAttribute(in, texcoords, 'v', 't', line)) {
        std::cerr << "Malformed texture coordinates: " << line << std::endl;
        return -1;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after texture coordinates" << std::endl;
        return -1;
    }

    // Read vertex normal data
    std::vector<Vec3> normals;
    if (!parseVertexAttribute(in, normals, 'v', 'n', line)) {
        std::cerr << "Malformed vertex normals: " << line << std::endl;
        return -1;
    } else if (in.eof()) {
        std::cerr << "Unexpected end of file after vertex normals" << std::endl;
        return -1;
    }

    std::string vertices[4];   // Holds 1 triangle or quad
    std::string* triangles[6]; // Holds up to two triangles
    unsigned int locations[3];

    std::unordered_map<std::string, int> indexCache;
    int index = 0;

    do {
        // Skip lines that do not specify texture coordinates
        if (line.length() < 2 || line[0] != 'f')
            continue;

        // Parse the texture coordinate data
        int deg = tokenize(vertices, line, 4);
        if (deg != 3 && deg != 4) {
            std::cerr << "All faces must be triangles or quads: " << line << std::endl;
            return -1;
        }

        int n = 3;
        triangles[0] = vertices; triangles[1] = vertices + 1; triangles[2] = vertices + 2;
        if (deg == 4) { // Triangulate quads
            n = 6;
            triangles[3] = vertices; triangles[4] = vertices + 2; triangles[5] = vertices + 3;
        }

        for (int i = 0; i < n; ++i) {
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
                    return -1;
                }

                // Write vertex data
                if (!addDataToBuffer(vertexData, locations[0], positions) ||
                    (attrs > 1 && !addDataToBuffer(vertexData, locations[1], texcoords, disableTexture, true)) ||
                    (attrs > 2 && !addDataToBuffer(vertexData, locations[2], normals, disableNormal))) {
                    std::cerr << "Vertex data out of bounds: " << v << std::endl;
                    return -1;
                }

                // Enter and cache the vertex
                elementData.push_back(index);
                indexCache[v] = index++;
            }
        }
    } while (std::getline(in, line));

    int stride = 3;
    if (texcoords.size() && !disableTexture)
        stride += 2;
    if (normals.size() && !disableNormal)
        stride += 3;
    return stride;
}

int main(int argc, char* argv[])
{
    std::ifstream inputFile;
    std::ofstream outputFile;
    // Read input file from first argument
    if (argc > 1) {
        inputFile.open(argv[1], std::ifstream::in);
        if (!inputFile.is_open()) {
            std::cerr << "Could not open file " << argv[1] << std::endl;
            return -1;
        }
    }
    // Read output file from second argument
    if (argc > 2) {
        outputFile.open(argv[2], std::ofstream::out);
        if (!outputFile.is_open()) {
            std::cerr << "Could not open file " << argv[2] << std::endl;
            return -1;
        }
    }
    // Read remaining arguments
    std::unordered_map<std::string, bool> args;
    for (int i = 3; i < argc; ++i)
        args[argv[i]] = i;
    bool disableTexture = args.count("--no-texture");
    bool disableNormal = args.count("--no-normal");
    std::istream& input = argc > 1 ? inputFile : std::cin;
    std::ostream& output = argc > 2 ? outputFile : std::cout;

    std::vector<double> vbo, ebo;
    int stride = objToJs(input, vbo, ebo, disableTexture, disableNormal);
    if (stride <= 0) {
        return -1;
    }

    output.precision(5);
    output << "let vbo = [";
    for (int i = 0, l = (int)vbo.size(); i < l; ++i) {
        if (i % stride == 0)
            output << "\n   ";
        output << ' ' << vbo[i] << ',';
    }
    output << "\n];\n\n";

    output << "let ebo = [";
    for (int i = 0, l = (int)ebo.size(); i < l; ++i) {
        if (i % 3 == 0)
            output << "\n   ";
        output << ' ' << ebo[i] << ',';
    }
    output << "\n];\n\n";

    if (inputFile.is_open())
        inputFile.close();
    if (outputFile.is_open())
        outputFile.close();
    return 0;
}
