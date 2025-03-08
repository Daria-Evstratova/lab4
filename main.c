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

GLuint bgTexture = 0;  // Переменная для текстуры фона
bool showBackground = false; // Флаг, указывающий, показывать ли фон
GLuint playerTexture = 0; // Текстура спрайта игрока
bool showPlayer = false; // Флаг, показывающий, отображать ли спрайт

float playerPosX = 100.0f; // Позиция игрока по оси X
float playerPosY = 200.0f; // Позиция игрока по оси Y

// Добавим глобальные переменные
bool isMoving = false; // Флаг движения
bool isFacingRight = true; // Направление персонажа
float currentFrame = 0.0f; // Текущий кадр анимации
float animationSpeed = 0.2f; // Скорость анимации
float moveSpeed = 5.0f; // Скорость движения

GLuint startBgTexture = 0;  // Текстура начального фона

bool playButtonVisible = true;
bool pauseMenuVisible = false;

float velocityY = 0.0f;        // Вертикальная скорость
float gravity = 0.5f;          // Сила гравитации
float jumpForce = -12.0f;      // Сила прыжка (отрицательная, так как ось Y направлена вниз)
bool isJumping = false;        // Флаг прыжка
float groundLevel = 600.0f - 80.0f;    // 600 (высота окна) - 80 (высота спрайта)

// Определяем размеры блока и символы для карты
#define BLOCK_SIZE 32.0f
#define BLOCK_CHAR 'B'
#define EMPTY_CHAR ' '

// Структура для уровня
typedef struct {
    char** data;
    int width;
    int height;
} Level;

Level currentLevel;

// Обновляем карту уровня (25x19 блоков, чтобы поместились все границы)
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

// Добавляем глобальный флаг для отображения уровня
bool showLevel = false;

// Функция для инициализации уровня
void InitLevel() {
    currentLevel.height = sizeof(levelData) / sizeof(levelData[0]);
    currentLevel.width = strlen(levelData[0]);

    // Выделяем память для данных уровня
    currentLevel.data = (char**)malloc(currentLevel.height * sizeof(char*));
    for (int i = 0; i < currentLevel.height; i++) {
        currentLevel.data[i] = (char*)malloc(currentLevel.width + 1);
        strcpy(currentLevel.data[i], levelData[i]);
    }

    // Корректируем начальную позицию игрока с учетом нового размера
    playerPosX = BLOCK_SIZE * 3;
    playerPosY = BLOCK_SIZE * 14; // Немного выше, чтобы компенсировать больший размер
}

// Функция для освобождения памяти уровня
void FreeLevel() {
    for (int i = 0; i < currentLevel.height; i++) {
        free(currentLevel.data[i]);
    }
    free(currentLevel.data);
}

// функция для проверки коллиззии с блоками
bool CheckCollision(float x, float y, float width, float height) {
    // Получаем координаты блоков, с которыми может быть столкновение
    int startX = (int)(x / BLOCK_SIZE);
    int endX = (int)((x + 64.0f - 1) / BLOCK_SIZE);
    int startY = (int)(y / BLOCK_SIZE);
    int endY = (int)((y + 64.0f - 1) / BLOCK_SIZE);

    // Проверяем каждый блок
    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            if (i >= 0 && i < currentLevel.height && j >= 0 && j < currentLevel.width) {
                if (currentLevel.data[i][j] == BLOCK_CHAR) {
                    // Проверяем точное пересечение с блоком
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
// проверяем находится ли игрок стоит на блоке
bool IsOnGround(float x, float y, float width, float height) {
    // Проверяем блоки непосредственно под персонажем
    int startX = (int)(x / BLOCK_SIZE);
    int endX = (int)((x + 64.0f - 1) / BLOCK_SIZE);
    int checkY = (int)((y + 64.0f) / BLOCK_SIZE);

    for (int j = startX; j <= endX; j++) {
        if (checkY >= 0 && checkY < currentLevel.height && j >= 0 && j < currentLevel.width) {
            if (currentLevel.data[checkY][j] == BLOCK_CHAR) {
                float blockTop = checkY * BLOCK_SIZE;
                // Проверяем, что персонаж действительно стоит на блоке
                if (y + height >= blockTop - 1 && y + height <= blockTop + 1) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Функция для отрисовки уровня
void RenderLevel() {
    glColor3f(0.3f, 0.0f, 0.4f);

    for (int i = 0; i < currentLevel.height; i++) {
        for (int j = 0; j < currentLevel.width; j++) {
            // рисуем уровне по карте. рисуем квадарты для B символов
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

// загрузщка текстуры
GLuint LoadTexture(const char *filename)
{
    int width, height, channels;
    // используем библиотеку stb для загрузки картинки
    unsigned char *image = stbi_load(filename, &width, &height, &channels, 4); // Загружаем с 4 каналами (RGBA)

    if (!image) {
        printf("Ошибка загрузки изображения: %s\n", filename);
        return 0;
    }

    // создаем текстуру в openGL
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    stbi_image_free(image);  // Освобождаем память

    return texture; // Возвращаем ID текстуры
}

// Структура кнопки
typedef struct
{
    char *name;      // Динамическая строка
    float vert[8];   // 4 вершины по 2 координаты
    float buffer[50 * 10]; // Буфер для координат символов
    int num_quads;   // Количество квадов для текста
    float textPosX, textPosY, textS; // Позиция и масштаб текста
} Button;

// Глобальный массив кнопок которые сейчас видимые
Button *btn = NULL;
int btnCnt = 0;

// Создание и добавление кнопки в общий массив
int AddButton(const char *name, float x, float y, float textS, float padding)
{
    btnCnt++;
    btn = (Button*)realloc(btn, sizeof(Button) * btnCnt);
    if (!btn) return -1;

    Button *b = &btn[btnCnt - 1];

    // Выделяем память для имени кнопки
    b->name = (char*)malloc(strlen(name) + 1);
    if (!b->name) return -1;
    strcpy(b->name, name);

    // Получаем ширину и высоту текста
    float textWidth = stb_easy_font_width(name) * textS;
    float textHeight = stb_easy_font_height(name) * textS;

    // Рассчитываем размеры кнопки с дополнительным запасом
    float width = textWidth + padding * 3;
    float height = textHeight + padding * 3; // Увеличиваем высоту для запаса

    // Устанавливаем координаты кнопки
    float *vert = b->vert;
    vert[0] = vert[6] = x;
    vert[2] = vert[4] = x + width;
    vert[1] = vert[3] = y;
    vert[5] = vert[7] = y + height;

    // Генерируем текст для рендеринга
    b->num_quads = stb_easy_font_print(0, 0, name, 0, b->buffer, sizeof(b->buffer));

    // Вычисляем координаты текста
    b->textPosX = x + (width - textWidth) / 2.0; // Центрируем текст по X
    b->textPosY = y + (height - textHeight) / 2.0 + textS * 1.5; // Двигаем текст вверх

    b->textS = textS;

    return btnCnt - 1;
}
// удаляем кнопки из массива и освобождаем память
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

// Отрисовка кнопки
void ShowButton(int buttonId)
{
    if (buttonId < 0 || buttonId >= btnCnt) return;

    Button btn1 = btn[buttonId];

    // Устанавливаем массив вершин кнопки (4 точки для прямоугольника)
    glVertexPointer(2, GL_FLOAT, 0, btn1.vert);
    // Включаем использование массива вершин чтобы не использовать отдельно glVertex()
    glEnableClientState(GL_VERTEX_ARRAY);

    // Устанавливаем зеленый цвет для заливки кнопки
    glColor3f(0.2, 1, 0.2);
    // Рисуем заполненный прямоугольник кнопки
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Устанавливаем серый цвет для обводки
    glColor3f(0.5, 0.5, 0.5);
    // Устанавливаем толщину линии обводки
    glLineWidth(1);
    // Рисуем контур кнопки
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // Отключаем массив вершин после отрисовки кнопки
    glDisableClientState(GL_VERTEX_ARRAY);

    // Сохраняем текущую матрицу трансформации
    glPushMatrix();
    // Устанавливаем серый цвет для текста
    glColor3f(0.5, 0.5, 0.5);
    // Перемещаем систему координат в позицию текста
    glTranslatef(btn1.textPosX, btn1.textPosY, 0);
    // Масштабируем текст
    glScalef(btn1.textS, btn1.textS, 1);

    // Включаем массив вершин для текста
    glEnableClientState(GL_VERTEX_ARRAY);
    // Устанавливаем массив вершин текста (шаг 16 байт между вершинами)
    glVertexPointer(2, GL_FLOAT, 16, btn1.buffer);
    // Рисуем текст (каждая буква состоит из квадратов)
    glDrawArrays(GL_QUADS, 0, btn1.num_quads * 4);
    // Отключаем массив вершин
    glDisableClientState(GL_VERTEX_ARRAY);

    // Восстанавливаем предыдущую матрицу трансформации
    glPopMatrix();
}
// обработка нажитий на кнопки
void OnButtonClick(int buttonId)
{
    static bool isPaused = false;
    // если нажали на кнопку play
    if (strcmp(btn[buttonId].name, "Play") == 0) {
        bgTexture = LoadTexture("background.png");
        playerTexture = LoadTexture("player_spritesheet.png");
        if (playerTexture == 0) {
            printf("Ошибка загрузки текстуры спрайта.\n");
        }
        // Показываем все что нужно при нажатии Play
        showBackground = true;
        showPlayer = true;
        showLevel = true;

        // Скрываем кнопку Play и показываем игровые кнопки
        playButtonVisible = false;
        pauseMenuVisible = true;

        // Удаляем старые кнопки и создаем новые
        FreeButtons();
        AddButton("Pause", 20, 20, 1, 10);
        AddButton("Reset", 20, 80, 1, 10);
        AddButton("Jump", 20, 140, 1, 10);
        AddButton("Start", 20, 200, 1, 10);
        AddButton("Exit", 20, 260, 1, 10);
    }
    // кнпока выхода
    else if (strcmp(btn[buttonId].name, "Exit") == 0) {
        PostQuitMessage(0);
    }
    else if (strcmp(btn[buttonId].name, "Pause") == 0) {
        isPaused = true;
        printf("Game paused\n");
        // Останавливаем движение и анимацию
        isMoving = false;
        moveSpeed = 0.0f;
        animationSpeed = 0.0f;
        gravity = 0.0f;
        velocityY = 0.0f;  // Обнуляем вертикальную скорость при паузе
    }
    else if (strcmp(btn[buttonId].name, "Reset") == 0) {
        printf("Game reset\n");
        playerPosX = BLOCK_SIZE * 3;
        playerPosY = BLOCK_SIZE * 14; // Обновляем позицию сброса
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
        // Возобновляем игру
        isPaused = false;
        moveSpeed = 5.0f;
        animationSpeed = 0.2f;
        gravity = 0.5f;
    }
}
// проверка по координатам попали ли мы по нажатию на мышь
void CheckMouseClick(float mouseX, float mouseY)
{
    for (int i = 0; i < btnCnt; i++) {
        float *vert = btn[i].vert;
        if (mouseX >= vert[0] && mouseX <= vert[4] && mouseY >= vert[1] && mouseY <= vert[5]) {
            OnButtonClick(i);
        }
    }
}

// Отображение фона
void ShowBackground()
{
    glEnable(GL_TEXTURE_2D);

    // Устанавливаем белый цвет для текстуры
    // Это нужно чтобы текстура отображалась в своих оригинальных цветах
    glColor3f(1.0f, 1.0f, 1.0f);

    // Выбираем какую текстуру использовать:
    if (showPlayer) {
        // Если игра началась - используем игровой фон
        glBindTexture(GL_TEXTURE_2D, bgTexture);
    } else {
        // Если игра не началась - используем стартовый фон
        glBindTexture(GL_TEXTURE_2D, startBgTexture);
    }

    // Начинаем рисовать прямоугольник (4 вершины)
    glBegin(GL_QUADS);
        // Для каждой вершины указываем:
        // glTexCoord2f - координаты текстуры (от 0 до 1)
        // glVertex2f - координаты вершины на экране

        // Левый верхний угол
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        // Правый верхний угол
        glTexCoord2f(1, 0); glVertex2f(800, 0);
        // Правый нижний угол
        glTexCoord2f(1, 1); glVertex2f(800, 600);
        // Левый нижний угол
        glTexCoord2f(0, 1); glVertex2f(0, 600);
    glEnd();

    // Выключаем режим работы с текстурами
    glDisable(GL_TEXTURE_2D);
}

// Отображение спрайта игрока
void ShowPlayer()
{
    if (!showPlayer) return;

    glEnable(GL_TEXTURE_2D);
    // включает поддержку прозрачности в OpenGL. Без этого все пиксели будут непрозрачными.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, playerTexture);

    // изменяем размер игрока на 64x64 для более удобной коллизии с размерами блока 32x32
    float frameWidth = 64.0f;
    float frameHeight = 64.0f;

    // Вычисляем координаты текстуры для текущего кадра. Т.к текстура содержит анимации
    float texLeft = (int)currentFrame * (1.0f / 7.0f);
    float texRight = texLeft + (1.0f / 7.0f);

    // Отражаем текстуру по горизонтали если персонаж смотрит влево
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

    // Обновляем анимацию только если персонаж движется
    if (isMoving) {
        currentFrame += animationSpeed;
        if (currentFrame >= 7.0f) currentFrame = 0.0f;
    }
    else {
        currentFrame = 0;
    }
}

// Функция отрисовки сцены
void Render()
{
    // Очищаем буфер цвета (очищаем экран)
    glClear(GL_COLOR_BUFFER_BIT);

    // Отрисовываем фон (стартовый экран или игровой фон)
    ShowBackground();

    // Если нужно показывать уровень (после нажатия Play)
    if (showLevel) {
        RenderLevel();  // Отрисовываем все блоки уровня
    }

    // Отрисовываем кнопки, если они должны быть видны
    if (playButtonVisible || pauseMenuVisible) {
        // Проходим по всем кнопкам и отрисовываем их
        for (int i = 0; i < btnCnt; i++)
            ShowButton(i);
    }

    // Отрисовываем игрока
    ShowPlayer();

    // Меняем буферы (двойная буферизация)
    // Показываем то, что нарисовали
    SwapBuffers(wglGetCurrentDC());
}

// Функция для инициализации OpenGL
void EnableOpenGL(HWND hwnd, HDC *hDC, HGLRC *hRC)
{
    // Создаем структуру, описывающую формат пикселей
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  // размер структуры
        1,  // версия
        // Флаги, указывающие что:
        // - рисуем в окно
        // - используем OpenGL
        // - используем двойную буферизацию
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,  // формат цвета RGBA
        24,  // 24 бита на цвет (по 8 бит на красный, зеленый и синий)
        // остальные параметры по умолчанию
        0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0
    };

    // Получаем контекст устройства для окна
    *hDC = GetDC(hwnd);
    // Выбираем подходящий формат пикселей
    int pf = ChoosePixelFormat(*hDC, &pfd);
    // Устанавливаем выбранный формат
    SetPixelFormat(*hDC, pf, &pfd);
    // Создаем контекст рендеринга OpenGL
    *hRC = wglCreateContext(*hDC);
    // Делаем его текущим
    wglMakeCurrent(*hDC, *hRC);

    // Инициализируем уровень игры
    InitLevel();

    // Переключаемся на матрицу проекции
    glMatrixMode(GL_PROJECTION);
    // Сбрасываем матрицу (делаем её единичной)
    glLoadIdentity();
    // Устанавливаем 2D-координаты для окна:
    // - X от 0 до 800 (ширина окна)
    // - Y от 0 до 600 (высота окна)
    // - Z от -1 до 1 (глубина)
    glOrtho(0, 800, 600, 0, -1, 1);

    // Переключаемся на матрицу моделирования
    glMatrixMode(GL_MODELVIEW);
    // Сбрасываем и эту матрицу
    glLoadIdentity();
}

// Функция для деинициализации OpenGL. Собираем мусор
void DisableOpenGL(HWND hwnd, HDC hdc, HGLRC hglrc)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);

    FreeLevel();
}

// Основная программа
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

    // Загружаем начальный фон
    startBgTexture = LoadTexture("start_background.png");

    // Добавление кнопок
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
                // Сохраняем предыдущую позицию игрока
                float prevX = playerPosX;
                float prevY = playerPosY;

                // Обрабатываем нажатие клавиш внутри цикла отрисовки чтобы сделать обработку плавнее
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                    playerPosX -= moveSpeed;
                    isMoving = true;
                    isFacingRight = false;
                    // tсли была коллизия возвращаем игрока в предыдующ точку, не даем ему ему двигаться
                    if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        playerPosX = prevX;
                    }
                }
                // & 0x8000 - побитовое сравнение с маской, которое проверяет, нажата ли клавиша в данный момент
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                    playerPosX += moveSpeed;
                    isMoving = true;
                    isFacingRight = true;

                    if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        playerPosX = prevX;
                    }
                }

                // Проверяем прыжок через пробел
                if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                    if (!isJumping && IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                        velocityY = jumpForce;
                        isJumping = true;
                    }
                }

                // Применяем гравитацию
                velocityY += gravity;
                playerPosY += velocityY;

                // Проверяем коллизии по вертикали
                if (CheckCollision(playerPosX, playerPosY, 64.0f, 64.0f)) {
                    playerPosY = prevY;
                    if (velocityY > 0) { // Если падаем
                        isJumping = false;
                    }
                    velocityY = 0;
                }

                // Проверяем, приземлились ли мы
                if (IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                    if (velocityY > 0) { // Если падаем
                        isJumping = false;
                        velocityY = 0;
                    }
                }

                // Если не движемся, сбрасываем флаг движения
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

// Обработчик событий окна
// Такие как нажатие на клавиши клавиатуры,
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // событие закртыия окна
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
                    // обработка нажатия на пробел
                    case VK_SPACE:
                        // делаем прыжок только если игрок на платформах
                        if (showLevel && showPlayer && IsOnGround(playerPosX, playerPosY, 64.0f, 64.0f)) {
                            velocityY = jumpForce;
                            isJumping = true;
                        }
                        break;
                }
            }
            break;
        // обработка нажатия на кнопку
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
