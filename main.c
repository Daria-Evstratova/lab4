#include <windows.h>
#include <gl/gl.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb-master/stb_easy_font.h"
#include "stb-master/stb_image.h"

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND hwnd, HDC hdc, HGLRC hglrc);
void Render();

GLuint bgTexture = 0;  // ���������� ��� �������� ����
bool showBackground = false; // ����, �����������, ���������� �� ���
GLuint playerTexture = 0; // �������� ������� ������
bool showPlayer = false; // ����, ������������, ���������� �� ������

float playerPosX = 100.0f; // ������� ������ �� ��� X
float playerPosY = 200.0f; // ������� ������ �� ��� Y

// ������� ���������� ����������
bool isMoving = false; // ���� ��������
bool isFacingRight = true; // ����������� ���������
float currentFrame = 0.0f; // ������� ���� ��������
float animationSpeed = 0.2f; // �������� ��������
float moveSpeed = 5.0f; // �������� ��������

GLuint startBgTexture = 0;  // �������� ���������� ����

bool playButtonVisible = true;
bool pauseMenuVisible = false;

float velocityY = 0.0f;        // ������������ ��������
float gravity = 0.5f;          // ���� ����������
float jumpForce = -12.0f;      // ���� ������ (�������������, ��� ��� ��� Y ���������� ����)
bool isJumping = false;        // ���� ������
float groundLevel = 600.0f - 80.0f;    // 600 (������ ����) - 80 (������ �������)

// ���������� ������� ����� � ������� ��� �����
#define BLOCK_SIZE 32.0f
#define BLOCK_CHAR 'B'
#define EMPTY_CHAR ' '

// ��������� ��� ������
typedef struct {
    char** data;
    int width;
    int height;
} Level;

Level currentLevel;

// ��������� ����� ������ (25x19 ������, ����� ����������� ��� �������)
const char* levelData[] = {
    "BBBBBBBBBBBBBBBBBBBBBBBBB", // 0
    "B                       B", // 1
    "B                       B", // 2
    "B                       B", // 3
    "B                       B", // 4
    "B      BBBB             B", // 5
    "B                       B", // 6
    "B                   BBBBB", // 7
    "B                       B", // 8
    "B          BBB          B", // 9
    "B            B          B", // 10
    "B            BBBB       B", // 11
    "B                       B", // 12
    "B                       B", // 13
    "B                       B", // 14
    "B                  BBBBBB", // 15
    "B            BBBBBBBBBBBB", // 16
    "B       BBBBBBBBBBBBBBBBB", // 17
    "BBBBBBBBBBBBBBBBBBBBBBBBB"  // 18
};

// ��������� ���������� ���� ��� ����������� ������
bool showLevel = false;

// ������� ��� ������������� ������
void InitLevel() {
    currentLevel.height = sizeof(levelData) / sizeof(levelData[0]);
    currentLevel.width = strlen(levelData[0]);

    // �������� ������ ��� ������ ������
    currentLevel.data = (char**)malloc(currentLevel.height * sizeof(char*));
    for (int i = 0; i < currentLevel.height; i++) {
        currentLevel.data[i] = (char*)malloc(currentLevel.width + 1);
        strcpy(currentLevel.data[i], levelData[i]);
    }

    // ������������ ��������� ������� ������ � ������ ������ �������
    playerPosX = BLOCK_SIZE * 3;
    playerPosY = BLOCK_SIZE * 14; // ������� ����, ����� �������������� ������� ������
}

// ������� ��� ������������ ������ ������
void FreeLevel() {
    for (int i = 0; i < currentLevel.height; i++) {
        free(currentLevel.data[i]);
    }
    free(currentLevel.data);
}

// ������� ��� �������� ��������� � �������
bool CheckCollision(float x, float y, float width, float height) {
    // �������� ���������� ������, � �������� ����� ���� ������������
    int startX = (int)(x / BLOCK_SIZE);
    int endX = (int)((x + 64.0f - 1) / BLOCK_SIZE);
    int startY = (int)(y / BLOCK_SIZE);
    int endY = (int)((y + 64.0f - 1) / BLOCK_SIZE);

    // ��������� ������ ����
    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            if (i >= 0 && i < currentLevel.height && j >= 0 && j < currentLevel.width) {
                if (currentLevel.data[i][j] == BLOCK_CHAR) {
                    // ��������� ������ ����������� � ������
                    float blockLeft = j * BLOCK_SIZE;
                    float blockRight = (j + 1) * BLOCK_SIZE;
                    float blockTop = i * BLOCK_SIZE;
                    float blockBottom = (i + 1) * BLOCK_SIZE;

                    if (x + width > blockLeft && x < blockRight &&
                        y + height > blockTop && y < blockBottom) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
// ��������� ��������� �� ����� ����� �� �����
bool IsOnGround(float x, float y, float width, float height) {
    // ��������� ����� ��������������� ��� ����������
    int startX = (int)(x / BLOCK_SIZE);
    int endX = (int)((x + 64.0f - 1) / BLOCK_SIZE);
    int checkY = (int)((y + 64.0f) / BLOCK_SIZE);

    for (int j = startX; j <= endX; j++) {
        if (checkY >= 0 && checkY < currentLevel.height && j >= 0 && j < currentLevel.width) {
            if (currentLevel.data[checkY][j] == BLOCK_CHAR) {
                float blockTop = checkY * BLOCK_SIZE;
                // ���������, ��� �������� ������������� ����� �� �����
                if (y + height >= blockTop - 1 && y + height <= blockTop + 1) {
                    return true;
                }
            }
        }
    }
    return false;
}

// ������� ��� ��������� ������
void RenderLevel() {
    glColor3f(0.3f, 0.0f, 0.4f);

    for (int i = 0; i < currentLevel.height; i++) {
        for (int j = 0; j < currentLevel.width; j++) {
            // ������ ������ �� �����. ������ �������� ��� B ��������
            if (currentLevel.data[i][j] == BLOCK_CHAR) {
                glBegin(GL_QUADS);
                glVertex2f(j * BLOCK_SIZE, i * BLOCK_SIZE);
                glVertex2f((j + 1) * BLOCK_SIZE, i * BLOCK_SIZE);
                glVertex2f((j + 1) * BLOCK_SIZE, (i + 1) * BLOCK_SIZE);
                glVertex2f(j * BLOCK_SIZE, (i + 1) * BLOCK_SIZE);
                glEnd();
            }
        }
    }
}

// ��������� ��������
GLuint LoadTexture(const char *filename)
{
    int width, height, channels;
    // ���������� ���������� stb ��� �������� ��������
    unsigned char *image = stbi_load(filename, &width, &height, &channels, 4); // ��������� � 4 �������� (RGBA)

    if (!image) {
        printf("������ �������� �����������: %s\n", filename);
        return 0;
    }

    // ������� �������� � openGL
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    stbi_image_free(image);  // ����������� ������

    return texture; // ���������� ID ��������
}

// ��������� ������
typedef struct
{
    char *name;      // ������������ ������
    float vert[8];   // 4 ������� �� 2 ����������
    float buffer[50 * 10]; // ����� ��� ��������� ��������
    int num_quads;   // ���������� ������ ��� ������
    float textPosX, textPosY, textS; // ������� � ������� ������
} Button;

// ���������� ������ ������ ������� ������ �������
Button *btn = NULL;
int btnCnt = 0;

// �������� � ���������� ������ � ����� ������
int AddButton(const char *name, float x, float y, float textS, float padding)
{
    btnCnt++;
    btn = (Button*)realloc(btn, sizeof(Button) * btnCnt);
    if (!btn) return -1;

    Button *b = &btn[btnCnt - 1];

    // �������� ������ ��� ����� ������
    b->name = (char*)malloc(strlen(name) + 1);
    if (!b->name) return -1;
    strcpy(b->name, name);

    // �������� ������ � ������ ������
    float textWidth = stb_easy_font_width(name) * textS;
    float textHeight = stb_easy_font_height(name) * textS;

    // ������������ ������� ������ � �������������� �������
    float width = textWidth + padding * 3;
    float height = textHeight + padding * 3; // ����������� ������ ��� ������

    // ������������� ���������� ������
    float *vert = b->vert;
    vert[0] = vert[6] = x;
    vert[2] = vert[4] = x + width;
    vert[1] = vert[3] = y;
    vert[5] = vert[7] = y + height;

    // ���������� ����� ��� ����������
    b->num_quads = stb_easy_font_print(0, 0, name, 0, b->buffer, sizeof(b->buffer));

    // ��������� ���������� ������
    b->textPosX = x + (width - textWidth) / 2.0; // ���������� ����� �� X
    b->textPosY = y + (height - textHeight) / 2.0 + textS * 1.5; // ������� ����� �����

    b->textS = textS;

    return btnCnt - 1;
}
// ������� ������ �� ������� � ����������� ������
void FreeButtons()
{
    for (int i = 0; i < btnCnt; i++)
    {
        free(btn[i].name);
    }
    free(btn);
    btn = NULL;
    btnCnt = 0;
}

// ��������� ������
void ShowButton(int buttonId)
{
    if (buttonId < 0 || buttonId >= btnCnt) return;

    Button btn1 = btn[buttonId];

    // ������������� ������ ������ ������ (4 ����� ��� ��������������)
    glVertexPointer(2, GL_FLOAT, 0, btn1.vert);
    // �������� ������������� ������� ������ ����� �� ������������ �������� glVertex()
    glEnableClientState(GL_VERTEX_ARRAY);

    // ������������� ������� ���� ��� ������� ������
    glColor3f(0.2, 1, 0.2);
    // ������ ����������� ������������� ������
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // ������������� ����� ���� ��� �������
    glColor3f(0.5, 0.5, 0.5);
    // ������������� ������� ����� �������
    glLineWidth(1);
    // ������ ������ ������
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // ��������� ������ ������ ����� ��������� ������
    glDisableClientState(GL_VERTEX_ARRAY);

    // ��������� ������� ������� �������������
    glPushMatrix();
    // ������������� ����� ���� ��� ������
    glColor3f(0.5, 0.5, 0.5);
    // ���������� ������� ��������� � ������� ������
    glTranslatef(btn1.textPosX, btn1.textPosY, 0);
    // ������������ �����
    glScalef(btn1.textS, btn1.textS, 1);

    // �������� ������ ������ ��� ������
    glEnableClientState(GL_VERTEX_ARRAY);
    // ������������� ������ ������ ������ (��� 16 ���� ����� ���������)
    glVertexPointer(2, GL_FLOAT, 16, btn1.buffer);
    // ������ ����� (������ ����� ������� �� ���������)
    glDrawArrays(GL_QUADS, 0, btn1.num_quads * 4);
    // ��������� ������ ������
    glDisableClientState(GL_VERTEX_ARRAY);

    // ��������������� ���������� ������� �������������
    glPopMatrix();
}
// ��������� ������� �� ������
void OnButtonClick(int buttonId)
{
    static bool isPaused = false;
    // ���� ������ �� ������ play
    if (strcmp(btn[buttonId].name, "Play") == 0) {
        bgTexture = LoadTexture("background.png");
        playerTexture = LoadTexture("player_spritesheet.png");
        if (playerTexture == 0) {
            printf("������ �������� �������� �������.\n");
        }
        // ���������� ��� ��� ����� ��� ������� Play
        showBackground = true;
        showPlayer = true;
        showLevel = true;

        // �������� ������ Play � ���������� ������� ������
        playButtonVisible = false;
        pauseMenuVisible = true;

        // ������� ������ ������ � ������� �����
        FreeButtons();
        AddButton("Pause", 20, 20, 1, 10);
        AddButton("Reset", 20, 80, 1, 10);
        AddButton("Jump", 20, 140, 1, 10);
        AddButton("Start", 20, 200, 1, 10);
        AddButton("Exit", 20, 260, 1, 10);
    }
    // ������ ������
    else if (strcmp(btn[buttonId].name, "Exit") == 0) {
        PostQuitMessage(0);
    }
    else if (strcmp(btn[buttonId].name, "Pause") == 0) {
        isPaused = true;
        printf("Game paused\n");
        // ������������� �������� � ��������
        isMoving = false;
        moveSpeed = 0.0f;
        animationSpeed = 0.0f;
        gravity = 0.0f;
        velocityY = 0.0f;  // �������� ������������ �������� ��� �����
    }
    else if (strcmp(btn[buttonId].name, "Reset") == 0) {
        printf("Game reset\n");
        playerPosX = BLOCK_SIZE * 3;
        playerPosY = BLOCK_SIZE * 14; // ��������� ������� ������
        velocityY = 0.0f;
        isMoving = false;
        isFacingRight = true;
        currentFrame = 0.0f;
        isPaused = false;
        moveSpeed = 5.0f;
        animationSpeed = 0.2f;
        gravity = 0.5f;
    }
    else if (strcmp(btn[buttonId].name, "Jump") == 0) {
        if (!isJumping && IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
            velocityY = jumpForce;
            isJumping = true;
            printf("Jump executed\n");
        }
    }
    else if (strcmp(btn[buttonId].name, "Start") == 0) {
        printf("Game resumed\n");
        // ������������ ����
        isPaused = false;
        moveSpeed = 5.0f;
        animationSpeed = 0.2f;
        gravity = 0.5f;
    }
}
// �������� �� ����������� ������ �� �� �� ������� �� ����
void CheckMouseClick(float mouseX, float mouseY)
{
    for (int i = 0; i < btnCnt; i++) {
        float *vert = btn[i].vert;
        if (mouseX >= vert[0] && mouseX <= vert[4] && mouseY >= vert[1] && mouseY <= vert[5]) {
            OnButtonClick(i);
        }
    }
}

// ����������� ����
void ShowBackground()
{
    glEnable(GL_TEXTURE_2D);

    // ������������� ����� ���� ��� ��������
    // ��� ����� ����� �������� ������������ � ����� ������������ ������
    glColor3f(1.0f, 1.0f, 1.0f);

    // �������� ����� �������� ������������:
    if (showPlayer) {
        // ���� ���� �������� - ���������� ������� ���
        glBindTexture(GL_TEXTURE_2D, bgTexture);
    } else {
        // ���� ���� �� �������� - ���������� ��������� ���
        glBindTexture(GL_TEXTURE_2D, startBgTexture);
    }

    // �������� �������� ������������� (4 �������)
    glBegin(GL_QUADS);
        // ��� ������ ������� ���������:
        // glTexCoord2f - ���������� �������� (�� 0 �� 1)
        // glVertex2f - ���������� ������� �� ������

        // ����� ������� ����
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        // ������ ������� ����
        glTexCoord2f(1, 0); glVertex2f(800, 0);
        // ������ ������ ����
        glTexCoord2f(1, 1); glVertex2f(800, 600);
        // ����� ������ ����
        glTexCoord2f(0, 1); glVertex2f(0, 600);
    glEnd();

    // ��������� ����� ������ � ����������
    glDisable(GL_TEXTURE_2D);
}

// ����������� ������� ������
void ShowPlayer()
{
    if (!showPlayer) return;

    glEnable(GL_TEXTURE_2D);
    // �������� ��������� ������������ � OpenGL. ��� ����� ��� ������� ����� �������������.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, playerTexture);

    // �������� ������ ������ �� 64x64 ��� ����� ������� �������� � ��������� ����� 32x32
    float frameWidth = 64.0f;
    float frameHeight = 64.0f;

    // ��������� ���������� �������� ��� �������� �����. �.� �������� �������� ��������
    float texLeft = (int)currentFrame * (1.0f / 7.0f);
    float texRight = texLeft + (1.0f / 7.0f);

    // �������� �������� �� ����������� ���� �������� ������� �����
    if (!isFacingRight) {
        float temp = texLeft;
        texLeft = texRight;
        texRight = temp;
    }

    glBegin(GL_QUADS);
        glTexCoord2f(texLeft, 0.0f);  glVertex2f(playerPosX, playerPosY);
        glTexCoord2f(texRight, 0.0f); glVertex2f(playerPosX + frameWidth, playerPosY);
        glTexCoord2f(texRight, 1.0f); glVertex2f(playerPosX + frameWidth, playerPosY + frameHeight);
        glTexCoord2f(texLeft, 1.0f);  glVertex2f(playerPosX, playerPosY + frameHeight);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    // ��������� �������� ������ ���� �������� ��������
    if (isMoving) {
        currentFrame += animationSpeed;
        if (currentFrame >= 7.0f) currentFrame = 0.0f;
    }
    else {
        currentFrame = 0;
    }
}

// ������� ��������� �����
void Render()
{
    // ������� ����� ����� (������� �����)
    glClear(GL_COLOR_BUFFER_BIT);

    // ������������ ��� (��������� ����� ��� ������� ���)
    ShowBackground();

    // ���� ����� ���������� ������� (����� ������� Play)
    if (showLevel) {
        RenderLevel();  // ������������ ��� ����� ������
    }

    // ������������ ������, ���� ��� ������ ���� �����
    if (playButtonVisible || pauseMenuVisible) {
        // �������� �� ���� ������� � ������������ ��
        for (int i = 0; i < btnCnt; i++)
            ShowButton(i);
    }

    // ������������ ������
    ShowPlayer();

    // ������ ������ (������� �����������)
    // ���������� ��, ��� ����������
    SwapBuffers(wglGetCurrentDC());
}

// ������� ��� ������������� OpenGL
void EnableOpenGL(HWND hwnd, HDC *hDC, HGLRC *hRC)
{
    // ������� ���������, ����������� ������ ��������
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  // ������ ���������
        1,  // ������
        // �����, ����������� ���:
        // - ������ � ����
        // - ���������� OpenGL
        // - ���������� ������� �����������
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,  // ������ ����� RGBA
        24,  // 24 ���� �� ���� (�� 8 ��� �� �������, ������� � �����)
        // ��������� ��������� �� ���������
        0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0
    };

    // �������� �������� ���������� ��� ����
    *hDC = GetDC(hwnd);
    // �������� ���������� ������ ��������
    int pf = ChoosePixelFormat(*hDC, &pfd);
    // ������������� ��������� ������
    SetPixelFormat(*hDC, pf, &pfd);
    // ������� �������� ���������� OpenGL
    *hRC = wglCreateContext(*hDC);
    // ������ ��� �������
    wglMakeCurrent(*hDC, *hRC);

    // �������������� ������� ����
    InitLevel();

    // ������������� �� ������� ��������
    glMatrixMode(GL_PROJECTION);
    // ���������� ������� (������ � ���������)
    glLoadIdentity();
    // ������������� 2D-���������� ��� ����:
    // - X �� 0 �� 800 (������ ����)
    // - Y �� 0 �� 600 (������ ����)
    // - Z �� -1 �� 1 (�������)
    glOrtho(0, 800, 600, 0, -1, 1);

    // ������������� �� ������� �������������
    glMatrixMode(GL_MODELVIEW);
    // ���������� � ��� �������
    glLoadIdentity();
}

// ������� ��� ��������������� OpenGL. �������� �����
void DisableOpenGL(HWND hwnd, HDC hdc, HGLRC hglrc)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);

    FreeLevel();
}

// �������� ���������
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex;
    HWND hwnd;
    HDC hDC;
    HGLRC hRC;
    MSG msg;
    BOOL bQuit = FALSE;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "GLSample";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) return 0;

    hwnd = CreateWindowEx(0, "GLSample", "OpenGL Buttons", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 800, 640,
                          NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    EnableOpenGL(hwnd, &hDC, &hRC);

    // ��������� ��������� ���
    startBgTexture = LoadTexture("start_background.png");

    // ���������� ������
    AddButton("Play", 20, 20, 1, 10);
    playButtonVisible = true;
    pauseMenuVisible = false;

    while (!bQuit)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) bQuit = TRUE;
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (showLevel && showPlayer) {
                // ��������� ���������� ������� ������
                float prevX = playerPosX;
                float prevY = playerPosY;

                // ������������ ������� ������ ������ ����� ��������� ����� ������� ��������� �������
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                    playerPosX -= moveSpeed;
                    isMoving = true;
                    isFacingRight = false;
                    // t��� ���� �������� ���������� ������ � ��������� �����, �� ���� ��� ��� ���������
                    if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        playerPosX = prevX;
                    }
                }
                // & 0x8000 - ��������� ��������� � ������, ������� ���������, ������ �� ������� � ������ ������
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                    playerPosX += moveSpeed;
                    isMoving = true;
                    isFacingRight = true;

                    if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        playerPosX = prevX;
                    }
                }

                // ��������� ������ ����� ������
                if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                    if (!isJumping && IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        velocityY = jumpForce;
                        isJumping = true;
                    }
                }

                // ��������� ����������
                velocityY += gravity;
                playerPosY += velocityY;

                // ��������� �������� �� ���������
                if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                    playerPosY = prevY;
                    if (velocityY > 0) { // ���� ������
                        isJumping = false;
                    }
                    velocityY = 0;
                }

                // ���������, ������������ �� ��
                if (IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                    if (velocityY > 0) { // ���� ������
                        isJumping = false;
                        velocityY = 0;
                    }
                }

                // ���� �� ��������, ���������� ���� ��������
                if (!(GetAsyncKeyState(VK_LEFT) & 0x8000) &&
                    !(GetAsyncKeyState(VK_RIGHT) & 0x8000)) {
                    isMoving = false;
                }
            }

            Render();
            Sleep(1);
        }
    }

    DisableOpenGL(hwnd, hDC, hRC);
    DestroyWindow(hwnd);
    FreeButtons();
    return msg.wParam;
}

// ���������� ������� ����
// ����� ��� ������� �� ������� ����������,
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // ������� �������� ����
        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_DESTROY:
            return 0;

        case WM_KEYDOWN:
            {
                switch (wParam)
                {
                    case VK_ESCAPE:
                        PostQuitMessage(0);
                        break;
                    // ��������� ������� �� ������
                    case VK_SPACE:
                        // ������ ������ ������ ���� ����� �� ����������
                        if (showLevel && showPlayer && IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                            velocityY = jumpForce;
                            isJumping = true;
                        }
                        break;
                }
            }
            break;
        // ��������� ������� �� ������
        case WM_LBUTTONDOWN:
            {
                float mouseX = LOWORD(lParam);
                float mouseY = HIWORD(lParam);
                CheckMouseClick(mouseX, mouseY);
            }
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
