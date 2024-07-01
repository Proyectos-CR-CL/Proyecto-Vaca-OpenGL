#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define WINDOW_WIDTH 1920.0f
#define WINDOW_HEIGHT 1080.0f
#define CAMERA_STEP 1.0f

const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;
    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aPos;
        TexCoord = aTexCoord;
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;

    uniform sampler2D texture1;

    void main()
    {
        vec4 texColor = texture(texture1, TexCoord);
        FragColor = texColor;
    }
)glsl";

const char* coneFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;

    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform vec3 lightColor;
    uniform vec3 objectColor;

    void main()
    {
        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * lightColor;

        // Diffuse 
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Specular
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;

        vec3 result = (ambient + diffuse + specular) * objectColor;
        FragColor = vec4(result, 0.5); // Cambiar la componente alfa para transparencia
    }
)glsl";

const char* laserVertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)glsl";

const char* laserFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(1.0, 0.0, 0.0, 0.35); // Color rojo con opacidad
    }
)glsl";


GLuint laserShaderProgram;
GLuint laserVAO, laserVBO;
GLuint shaderProgram;
GLuint coneShaderProgram;
class Camera* camera;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint loadShader(GLenum type, const char* source);
GLuint loadTexture(const std::string& path);

// Definición de un cono simple
std::vector<float> coneVertices;
std::vector<unsigned int> coneIndices;
const int coneResolution = 30;
const float coneHeight = 50.0f;  // Ajustar la altura del cono
const float coneRadius = 10.0f;  // Ajustar el radio del cono
float time_laser = 0.0f;

void setupLaserShader() {
    GLuint laserVertexShader = loadShader(GL_VERTEX_SHADER, laserVertexShaderSource);
    GLuint laserFragmentShader = loadShader(GL_FRAGMENT_SHADER, laserFragmentShaderSource);
    laserShaderProgram = glCreateProgram();
    glAttachShader(laserShaderProgram, laserVertexShader);
    glAttachShader(laserShaderProgram, laserFragmentShader);
    glLinkProgram(laserShaderProgram);

    // Comprobar errores de enlace
    int success;
    char infoLog[512];
    glGetProgramiv(laserShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(laserShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(laserVertexShader);
    glDeleteShader(laserFragmentShader);
}

void setupLaser() {
    glGenVertexArrays(1, &laserVAO);
    glGenBuffers(1, &laserVBO);

    glBindVertexArray(laserVAO);

    glBindBuffer(GL_ARRAY_BUFFER, laserVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void generateCone() {
    // Vertices
    coneVertices.push_back(0.0f); // Centro de la base (x)
    coneVertices.push_back(0.0f); // Centro de la base (y)
    coneVertices.push_back(0.0f); // Centro de la base (z)

    for (int i = 0; i <= coneResolution; ++i) {
        float angle = 2.0f * 3.14159265359f * float(i) / float(coneResolution);
        float x = coneRadius * cos(angle);
        float z = coneRadius * sin(angle);
        coneVertices.push_back(x); // Base vertex (x)
        coneVertices.push_back(0.0f); // Base vertex (y)
        coneVertices.push_back(z); // Base vertex (z)
    }

    // Tip of the cone
    coneVertices.push_back(0.0f); // (x)
    coneVertices.push_back(coneHeight); // (y)
    coneVertices.push_back(0.0f); // (z)

    // Indices for base
    for (int i = 1; i <= coneResolution; ++i) {
        coneIndices.push_back(0);
        coneIndices.push_back(i);
        coneIndices.push_back(i + 1);
    }

    // Indices for the sides
    int tipIndex = coneVertices.size() / 3 - 1;
    for (int i = 1; i <= coneResolution; ++i) {
        coneIndices.push_back(tipIndex);
        coneIndices.push_back(i);
        coneIndices.push_back(i + 1);
    }
}

class Model
{
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<unsigned int> indices;
    GLuint vao, vbo, ebo;
    std::vector<GLuint> textureIDs;

    void setUpVao()
    {
        std::cout << "Vertices: " << vertices.size() << std::endl;
        std::cout << "Indices: " << indices.size() << std::endl;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        if (!texcoords.empty())
        {
            GLuint texVBO;
            glGenBuffers(1, &texVBO);
            glBindBuffer(GL_ARRAY_BUFFER, texVBO);
            glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(float), texcoords.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    bool loadModel(const std::string& path)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::filesystem::path objPath = path;
        std::string baseDir = objPath.parent_path().string() + "/";

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), baseDir.c_str()))
        {
            std::cerr << "Error al cargar/parsear el archivo .obj: " << warn << err << std::endl;
            return false;
        }

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
                if (!attrib.texcoords.empty()) {
                    texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                    texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
                }
                indices.push_back(indices.size());
            }
        }

        setUpVao();

        // Cargar texturas desde el archivo .mtl
        for (const auto& material : materials)
        {
            if (!material.diffuse_texname.empty())
            {
                std::string texturePath = baseDir + material.diffuse_texname;
                textureIDs.push_back(loadTexture(texturePath));
            }
        }

        return true;
    }

public:
    Model(const std::string& path)
    {
        bool build = loadModel(path);
        if (!build)
            exit(1);
    }

    void draw()
    {
        for (const auto& textureID : textureIDs)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
        }
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

class Object
{
    glm::vec4 position;
    glm::mat4x4 transformation;
    Model* model;

public:
    Object(Model* _model, const glm::mat4x4& _transformation) :
        transformation(_transformation), model(_model)
    {
        position = transformation * glm::vec4(0.0f);
    }

    void updateTransformation(const glm::mat4x4& _transformation)
    {
        transformation = _transformation;
    }

    void draw()
    {
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(transformation));
        model->draw();
    }
};

class Camera
{
    glm::vec3 position;
    glm::vec3 center;

    glm::mat4x4 viewMatrix;
    glm::mat4x4 projMatrix;

    float fovy, aspect, near, far;

    void updateViewMatrix()
    {
        viewMatrix = glm::lookAt(position, center, glm::vec3(0.0f, 0.1f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
    }

public:
    Camera(const glm::vec3& _position, const glm::vec3& _center, float _fovy, float _aspect, float _near, float _far) :
        position(_position), center(_center), fovy(_fovy), aspect(_aspect), near(_near), far(_far)
    {
        updateViewMatrix();
        projMatrix = glm::perspective(fovy, aspect, near, far);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projMatrix));
    }

    void move(const glm::vec3& amount)
    {
        center += amount;
        position += amount;

        updateViewMatrix();
    }

    void turn(const glm::vec3& amount)
    {
        center += amount;

        updateViewMatrix();
    }

    glm::vec3 getCenter()
    {
        return center;
    }

    glm::vec3 getPosition()
    {
        return position;
    }

    void updateCameraPosition(float ufoX, float ufoY, float ufoZ, float cowY, bool cowAscending, bool cowAbducted, bool ufoRetreating, bool cameraStopped)
    {
        static glm::vec3 initialCameraPosition = glm::vec3(120.0f, 20.0f, 120.0f);
        static glm::vec3 initialCameraCenter = glm::vec3(-50.0f, 10.0f, 0.0f);

        if (cameraStopped) return;

        if (cowAscending && !cowAbducted) {
            move(glm::vec3(0.0f, 0.08f, 0.0f));
            turn(glm::vec3(0.0f, 0.08f, 0.0f));
        } else if (cowAbducted) {
            move(glm::vec3(0.0f, 0.05f, 0.0f));
			// move(glm::vec3(0.0f, 0.08f, 0.0f));
			// if (ufoY < 70.0f) { // Ajusta el valor 70.0f según tus necesidades
				// move(glm::vec3(0.0f, 0.12f, 0.0f)); // Ajusta el valor 0.1f según tus necesidades
			// }
        } else if (ufoRetreating) {
            move(glm::vec3(0.1f, 0.0f, 0.0f));
        } else {
            glm::vec3 ufoPosition = glm::vec3(ufoX, ufoY, ufoZ);
            glm::vec3 cameraPosition = getPosition();
            glm::vec3 directionToUFO = glm::normalize(ufoPosition - cameraPosition);
            move(directionToUFO * 0.05f);
            turn(directionToUFO * 0.05f);
        }
    }

    glm::mat4x4 getViewMatrix() const
    {
        return viewMatrix;
    }

    glm::mat4x4 getProjMatrix() const
    {
        return projMatrix;
    }
};

int main()
{
    // Inicializar GLFW
    if (!glfwInit()) {
        std::cerr << "Error al inicializar GLFW" << std::endl;
        return -1;
    }

    // Crear ventana
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Cargador de múltiples OBJ", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Error al crear la ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, processKeyInput);

    // Inicializar GLAD
    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cout << "Error al inicializar GLAD.\n";
        glfwTerminate();
        return -1;
    }

    // Compilar shaders
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Comprobar errores de enlace
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Compilar shaders para el cono
    GLuint coneVertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint coneFragmentShader = loadShader(GL_FRAGMENT_SHADER, coneFragmentShaderSource);
    coneShaderProgram = glCreateProgram();
    glAttachShader(coneShaderProgram, coneVertexShader);
    glAttachShader(coneShaderProgram, coneFragmentShader);
    glLinkProgram(coneShaderProgram);

    // Comprobar errores de enlace
    glGetProgramiv(coneShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(coneShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(coneVertexShader);
    glDeleteShader(coneFragmentShader);

	// Configurar shaders y láser
    setupLaserShader();
    setupLaser();

    // Generar el cono
    GLuint coneVAO, coneVBO, coneEBO;
    generateCone();

    // Crear y configurar VAO, VBO y EBO para el cono
    glGenVertexArrays(1, &coneVAO);
    glGenBuffers(1, &coneVBO);
    glGenBuffers(1, &coneEBO);

    glBindVertexArray(coneVAO);

    glBindBuffer(GL_ARRAY_BUFFER, coneVBO);
    glBufferData(GL_ARRAY_BUFFER, coneVertices.size() * sizeof(float), &coneVertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, coneEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, coneIndices.size() * sizeof(unsigned int), &coneIndices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Relative Path
    std::filesystem::path p = std::filesystem::current_path();
    int levels_path = 1;
    std::filesystem::path p_current;
    p_current = p.parent_path();

    for (int i = 0; i < levels_path; i++)
    {
        p_current = p_current.parent_path();
    }

    std::string vs_path, fs_path;

    std::stringstream ss;
    ss << std::quoted(p_current.string());
    std::string out;
    ss >> std::quoted(out);

    std::cout << "\nCurrent path: " << out << "\n";

    // Cargar modelos
    std::vector<Model> models;
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\tree_in_OBJ.obj"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\002_obj.obj"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\10438_Circular_Grass_Patch_v1_iterations-2.obj"));
    
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\cowTM08New00RTime02.obj"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\Low_poly_UFO.obj"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_01\\modelos\\10438_Circular_Grass_Patch_v1_iterations-1.obj"));
    std::vector<Object> objects;



    float treeStartX = -250.0f; // Ajusta el inicio en X para que los árboles estén más lejos en la dirección de la vista de la cámara
    float treeStartZ = -200.0f; // Ajusta el inicio en Z para que los árboles estén en un área visible desde la cámara
    float treeSpacing = 100.0f; // Espaciado entre árboles
    int treeRows = 5; // Número de filas de árboles
    int treeCols = 5; // Número de columnas de árboles
    float treeYOffset = -5.0f; // Ajusta este valor para hundir los árboles en el eje Y

    for (int i = 0; i < treeRows; ++i) {
        for (int j = 0; j < treeCols; ++j) {
            float treeX = treeStartX + i * treeSpacing;
            float treeZ = treeStartZ + j * treeSpacing;
            // Evitar interferir con el área del OVNI y la vaca (suponiendo que el OVNI y la vaca están alrededor de (0, 0, 50))
            if (treeX > -60.0f && treeX < 60.0f && treeZ > 40.0f && treeZ < 60.0f) continue;
            objects.push_back(
                Object(&models[0], 
                    glm::scale(
                        glm::translate(
                            glm::mat4x4(1.0f),
                            glm::vec3(treeX, treeYOffset, treeZ)
                        ),
                        glm::vec3(0.45f) // Reducción del tamaño a la mitad
                    )
                )
            );
        }
    }

    
  float skyStartX = 0.0f; // La posición en X es central respecto a la posición inicial de la cámara o del escenario
float skyStartZ = 0.0f; // La posición en Z es también central
float skyHeight = 250.0f; // Altura sobre el suelo o la cámara
float skyScale = 10.0f; // Escala del cielo, ajusta este valor según sea necesario para el tamaño deseado

// Asumiendo que el modelo del cielo es el último que se agregó a la lista de modelos
int skyModelIndex = 5; // Cambiar esto según el índice correcto del modelo de cielo en 'models'

// Crear la matriz de transformación para el modelo de cielo
glm::mat4 skyModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(skyStartX, skyHeight, skyStartZ));
skyModelMatrix = glm::rotate(skyModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotar 90 grados negativos en el eje X
skyModelMatrix = glm::rotate(skyModelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));  // Rotar 90 grados en el eje Z
skyModelMatrix = glm::rotate(skyModelMatrix, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotar 180 grados en el eje Y
skyModelMatrix = glm::scale(skyModelMatrix, glm::vec3(skyScale, skyScale, skyScale)); // Escalar el modelo

// Agregar el objeto de cielo a la lista de objetos con la transformación adecuada
objects.push_back(
    Object(&models[skyModelIndex],
        skyModelMatrix
    )
);




    float grassStartX = -1500.0f; // Ajusta según el tamaño del escenario
    float grassStartZ = -1500.0f; // Ajusta según el tamaño del escenario
    float grassSpacing = 100.0f;
    int grassRows = 20; // Número de filas de pasto
    int grassCols = 20; // Número de columnas de pasto

    for (int i = 0; i < grassRows; ++i) {
        for (int j = 0; j < grassCols; ++j) {
            float grassX = grassStartX + i * grassSpacing;
            float grassZ = grassStartZ + j * grassSpacing;
            objects.push_back(
                Object(&models[2],
                    glm::rotate(
                        glm::translate(
                            glm::mat4x4(1.0f),
                            glm::vec3(grassX, -10.0f, grassZ)
                        ),
                        glm::radians(-90.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)
                    )
                )
            );
        }
    }

    // Primera montaña
    float mountainGrassStartX = -1200.0f; // Ajusta según la posición de la casa y la cámara
    float mountainGrassStartZ = -1200.0f; // Ajusta según la posición de la casa y la cámara
    float mountainGrassSpacing = 70.0f; // Espaciado entre los pastos en la montaña
    int mountainGrassRows = 15; // Número de filas de pasto en la montaña
    int mountainGrassCols = 15; // Número de columnas de pasto en la montaña
    float mountainHeightScale = 50.0f; // Escala de altura para crear una montaña más alta
    float mountainBaseHeight = -20.0f; // Altura de inicio de las montañas, valor negativo para comenzar más abajo

    for (int i = 0; i < mountainGrassRows; ++i) {
        for (int j = 0; j < mountainGrassCols; ++j) {
            float grassX = mountainGrassStartX + i * mountainGrassSpacing;
            float grassZ = mountainGrassStartZ + j * mountainGrassSpacing;
            float distanceFromCenter = glm::length(glm::vec2(grassX - mountainGrassStartX, grassZ - mountainGrassStartZ));
            float grassY = glm::sin(glm::radians((float)distanceFromCenter / (float)(mountainGrassRows * mountainGrassSpacing) * 180.0f)) * mountainHeightScale + mountainBaseHeight;
            objects.push_back(
                Object(&models[2],
                    glm::rotate(
                        glm::translate(
                            glm::mat4x4(1.0f),
                            glm::vec3(grassX, grassY, grassZ)
                        ),
                        glm::radians(-90.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)
                    )
                )
            );
        }
    }

    // Segunda montaña
    float secondMountainGrassStartX2 = -1500.0f; // Ajusta según la posición de la casa y la cámara
    float secondMountainGrassStartZ2 = 0.0f; // Ajusta según la posición de la casa y la cámara
    float secondMountainGrassSpacing2 = 70.0f; // Espaciado entre los pastos en la montaña
    int secondMountainGrassRows2 = 15; // Número de filas de pasto en la montaña
    int secondMountainGrassCols2 = 15; // Número de columnas de pasto en la montaña
    float secondMountainHeightScale2 = 40.0f; // Escala de altura para crear una montaña más alta
    float secondMountainBaseHeight2 = -20.0f; // Altura de inicio de las montañas, valor negativo para comenzar más abajo

    for (int i = 0; i < secondMountainGrassRows2; ++i) {
        for (int j = 0; j < secondMountainGrassCols2; ++j) {
            float grassX = secondMountainGrassStartX2 + i * secondMountainGrassSpacing2;
            float grassZ = secondMountainGrassStartZ2 + j * secondMountainGrassSpacing2;
            float distanceFromCenter = glm::length(glm::vec2(grassX - secondMountainGrassStartX2, grassZ - secondMountainGrassStartZ2));
            float grassY = glm::sin(glm::radians((float)distanceFromCenter / (float)(secondMountainGrassRows2 * secondMountainGrassSpacing2) * 180.0f)) * secondMountainHeightScale2 + secondMountainBaseHeight2;
            objects.push_back(
                Object(&models[2],
                    glm::rotate(
                        glm::translate(
                            glm::mat4x4(1.0f),
                            glm::vec3(grassX, grassY, grassZ)
                        ),
                        glm::radians(-90.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)
                    )
                )
            );
        }
    }

    // Agregar otros objetos (casa, vaca, OVNI)
    objects.push_back(
        Object(&models[1], // House
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(-70.0f, 0.0f, -30.0f)
                ), 
                glm::vec3(4.9f)
            )
        )
    );

    // Asegurarse de que la vaca y el OVNI sean los últimos objetos en la lista de `objects`
    objects.push_back(
        Object(&models[3], // cow
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.2f),
                    glm::vec3(0.0f, 5.0f, 50.0f) 
                ),
                glm::vec3(0.2f)
            )
        )
    );

    objects.push_back(
        Object(&models[4], // UFO
            glm::translate(
                glm::scale(
                    glm::mat4x4(1.0f),
                    glm::vec3(1.0f) // Ajusta la escala si es necesario
                ),
                glm::vec3(-50.0f, 50.0f, 50.0f) // Ajusta la posición inicial para que esté arriba
            )
        )
    );

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Habilitar blending para transparencia
    glClearColor(0.05f, 0.05f, 0.2f, 1.0f);
	time_laser += 0.05f; // Ajusta la velocidad del movimiento del láser aquí

    // Configuración inicial de la cámara
    camera = new Camera(glm::vec3(120.0f, 20.0f, 120.0f), glm::vec3(-50.0f, 10.0f, 0.0f), glm::radians(45.0f), WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 1000.0f);

    float ufoRotationAngle = 0.0f;
    float ufoPositionY = 50.0f;
    float ufoPositionX = -50.0f;
    float ufoScale = 1.0f;
    float cowPositionOffsetY = 5.0f;
    float cowScale = 1.2f;
    float cowRotationAngle = 0.0f;
    bool ufoDescending = true;
    bool ufoMovingRight = false;
    bool cowAscending = false;
    bool cowRotating = false;
    bool cowAbducted = false; // Variable para controlar si la vaca ha sido completamente abducida
    bool ufoRetreating = false; // Variable para controlar el retiro del UFO
    bool ufoExiting = false; // Variable para controlar la salida del OVNI
    bool cameraStopped = false; // Variable para controlar si la cámara se detiene

    glm::vec3 lightPos(50.0f, 50.0f, 50.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    glm::vec3 objectColor(1.0f, 1.0f, 0.0f);

    // Bucle de renderizado
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.12f, 0.1f, 1.0f);


        // Actualizar rotación del OVNI
        ufoRotationAngle += 0.03f;
        if (ufoRotationAngle > 360.0f)
            ufoRotationAngle -= 360.0f;

        // Actualizar posición del OVNI
        if (ufoDescending) {
            ufoPositionY -= 0.7f;
            if (ufoPositionY <= 30.0f) {
                ufoDescending = false;
                ufoMovingRight = true;
            }
        }

        if (ufoMovingRight) {
            ufoPositionX += 0.5f;
            if (ufoPositionX >= 0.0f) {
                ufoMovingRight = false;
                cowAscending = true;
            }
        }

        // Actualizar posición y escala de la vaca
        if (cowAscending) {
            cowPositionOffsetY += 0.7f; // Incrementar la velocidad de ascenso
            cowScale -= 0.025f; // Reducir la velocidad de cambio de escala
            if (cowScale <= 0.0f) {
                cowScale = 0.0f;
                cowAscending = false;
                cowAbducted = true; // Indicar que la vaca ha sido completamente abducida
                ufoRetreating = true; // Activar el retiro del UFO
            }
            // Comenzar la rotación de la vaca cuando empiece a ascender
            if (cowPositionOffsetY >= 6.0f && !cowRotating) {
                cowRotating = true;
            }
        }

        if (cowRotating) {
            cowRotationAngle += 0.05f; // Velocidad de rotación de la vaca
            if (cowRotationAngle > glm::radians(180.0f)) {
                cowRotationAngle = 0.0f;
                cowRotating = false;
            }
        }

        // Lógica de retiro del UFO
        if (ufoRetreating) {
            ufoPositionY += 0.15f; // Incrementa la posición Y del UFO para que se retire
            if (ufoPositionY >= 70.0f) { // Verifica si el UFO ha salido de la escena
                ufoRetreating = false; // Detén el movimiento del UFO cuando esté fuera de la escena
                ufoExiting = true; // Activar la salida del OVNI hacia la izquierda
            }
        }

        // Lógica de salida del OVNI hacia la izquierda y reducción de tamaño
        if (ufoExiting) {
            ufoPositionX -= 0.05f; // Mover el OVNI hacia la izquierda
            ufoScale -= 0.008f; // Reducir el tamaño del OVNI
            if (ufoScale <= 0.0f) {
                ufoScale = 0.0f;
                ufoExiting = false; // Detener la salida del OVNI cuando desaparezca
                cameraStopped = true; // Detener la cámara cuando el OVNI desaparezca
            }
        }

        glm::mat4 ufoModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ufoPositionX, ufoPositionY, 50.0f));
        ufoModelMatrix = glm::rotate(ufoModelMatrix, ufoRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        ufoModelMatrix = glm::scale(ufoModelMatrix, glm::vec3(ufoScale));
        objects[objects.size() - 1].updateTransformation(ufoModelMatrix);

        glm::mat4 cowModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, cowPositionOffsetY, 50.0f));
        cowModelMatrix = glm::scale(cowModelMatrix, glm::vec3(cowScale));
        if (cowRotating) {
            cowModelMatrix = glm::rotate(cowModelMatrix, cowRotationAngle, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        objects[objects.size() - 2].updateTransformation(cowModelMatrix);

        // // Actualizar la posición de la cámara
        // camera->updateCameraPosition(ufoPositionX, ufoPositionY, 50.0f, cowPositionOffsetY, cowAscending, cowAbducted, ufoRetreating, cameraStopped);
		
		bool coneActive = !ufoDescending && !cowAbducted;

        // Renderizar objetos
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera->getViewMatrix()));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(camera->getProjMatrix()));
        for (size_t i = 0; i < objects.size(); ++i)
        {
            objects[i].draw();
        }
		
		if (!cowAscending && !cowAbducted && !coneActive) {
			glm::vec3 laserVertices[] = {
				glm::vec3(ufoPositionX, ufoPositionY + 10.0f, 50.0f),         // Punto de inicio en el OVNI
				glm::vec3(ufoPositionX + 10.0f * cos(time_laser), ufoPositionY - coneHeight, 50.0f + 10.0f * sin(time_laser)) // Punto de finalización en el suelo en movimiento circular
			};

			glUseProgram(laserShaderProgram);
			glUniformMatrix4fv(glGetUniformLocation(laserShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera->getViewMatrix()));
			glUniformMatrix4fv(glGetUniformLocation(laserShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(camera->getProjMatrix()));
			glm::mat4 laserModelMatrix = glm::mat4(1.0f);
			glUniformMatrix4fv(glGetUniformLocation(laserShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(laserModelMatrix));

			glBindBuffer(GL_ARRAY_BUFFER, laserVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(laserVertices), laserVertices);
			glBindVertexArray(laserVAO);

			// Ajustar el ancho del láser
			glLineWidth(5.0f); // Cambia este valor para ajustar el grosor del láser

			glDrawArrays(GL_LINES, 0, 2);
			glBindVertexArray(0);
		}

        // Dibujar el cono una vez que el OVNI ha terminado de bajar y antes de que la vaca sea completamente abducida
        if (coneActive) {
            glUseProgram(coneShaderProgram);
            glUniform3fv(glGetUniformLocation(coneShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
            glUniform3fv(glGetUniformLocation(coneShaderProgram, "viewPos"), 1, glm::value_ptr(camera->getPosition()));
            glUniform3fv(glGetUniformLocation(coneShaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
            glUniform3fv(glGetUniformLocation(coneShaderProgram, "objectColor"), 1, glm::value_ptr(objectColor));
            glm::mat4 coneModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ufoPositionX, ufoPositionY - coneHeight / 2.0f, 50.0f)); // Ajustar la posición del cono
            coneModelMatrix = glm::rotate(coneModelMatrix, ufoRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(coneShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(coneModelMatrix));
            glUniformMatrix4fv(glGetUniformLocation(coneShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(camera->getViewMatrix()));
            glUniformMatrix4fv(glGetUniformLocation(coneShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(camera->getProjMatrix()));
            glBindVertexArray(coneVAO);
            glDrawElements(GL_TRIANGLES, coneIndices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
		
		// Actualizar la posición de la cámara
        camera->updateCameraPosition(ufoPositionX, ufoPositionY, 50.0f, cowPositionOffsetY, cowAscending, cowAbducted, ufoRetreating, cameraStopped);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, true);

    if (action == GLFW_PRESS && key == GLFW_KEY_LEFT)
        camera->move(-1.0f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_RIGHT)
        camera->move(CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_UP)
        camera->move(CAMERA_STEP * glm::normalize(camera->getCenter() - camera->getPosition()));
    if (action == GLFW_PRESS && key == GLFW_KEY_DOWN)
        camera->move(-1.0f * CAMERA_STEP * glm::normalize(camera->getCenter() - camera->getPosition()));

    if (action == GLFW_PRESS && key == GLFW_KEY_A)
        camera->turn(-0.5f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_D)
        camera->turn(0.5f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
}

GLuint loadShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint loadTextures(const char* filename)
{
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (!data){
        std::cerr << "Failed to load texture" << std::endl;
        return 0;
    }
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D,textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return textureID;
}

GLuint loadTexture(const std::string& path)
{
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Error al cargar la textura: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}