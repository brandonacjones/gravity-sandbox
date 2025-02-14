#include "raylib.h"
#include <string>
#include <vector>
#include <iostream>
#include <cmath>

const int SCREEN_WIDTH = 1400;
const int SCREEN_HEIGHT = 1000;

// UI Parameters
const Color UI_MENU_BG = GetColor(0x262626FF);
const Color UI_CHECKBOX_BG = GetColor(0x3A3A3AFF);
const Color UI_CHECKBOX_ACTIVE = GetColor(0xBB86FCFF);
const Color UI_CHECKBOX_INACTIVE = GetColor(0x555555FF);
const Color UI_TEXT = GetColor(0xE0E0E0FF);
const Color UI_BUTTON_UNCLKD = GetColor(0xE0E0E0FF);
const Color UI_BUTTON_UNCLKD_TXT = GetColor(0x333333FF);
const Color UI_BUTTON_HVR = GetColor(0xBDBDBDFF);
const Color UI_BUTTON_HVR_TXT = GetColor(0x000000FF);
const Color UI_BUTTON_CLKD = GetColor(0x9E9E9EFF);
const Color UI_BUTTON_CLKD_TXT = GetColor(0xFFFFFFFF);

// Sim Parameters
const int SIM_WIDTH = SCREEN_WIDTH - 400;
const int SIM_HEIGHT = SCREEN_HEIGHT;
const float SIM_WIDTH_HALF = SIM_WIDTH / 2.0f;
const float SIM_HEIGHT_HALF = SIM_HEIGHT / 2.0f;
const float G = 6.67430e-8;
const float MIN_DISTANCE_SQUARED = 0.1f; // Threshold to avoid division by zero in calculations.
const float VECTOR_DRAW_SCALE = 50.0f;
const float GRID_SQUARE_SIZE = 5.0f;
const Color SIM_BG_COL = GetColor(0x020202FF);
const Color SIM_BDY_COL = GetColor(0xC9C9C9FF);
const Color SIM_SPAWN_BDY_COL = GetColor(0xB09C02FF);
const Color SIM_SPAWN_VEL_COL = GetColor(0xB09C02FF); //0xE3D98DFF
bool showVectors = false;
bool showField = false;

enum State {
	DEFAULT,
	DRAWING
};

// UI

struct Button {
	const std::string message;
	const float x;
	const float y;
	const float width;
	const float height;	

	Button(float x, float y, float h, float w, const std::string& m) :
		x(x),
		y(y),
		height(h),
		width(w),
		message(m)
	{}

	void DrawButton() const {
		int textWidth = MeasureText(message.c_str(), height / 3.0f);
		Vector2 mousePos = GetMousePosition();
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			DrawRectangle(x, y, width, height, UI_BUTTON_CLKD);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 3.0f, UI_BUTTON_CLKD_TXT);
		}
		else if (CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			DrawRectangle(x, y, width, height, UI_BUTTON_HVR);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 3.0f, UI_BUTTON_HVR_TXT);
		}
		else {
			DrawRectangle(x, y, width, height, UI_BUTTON_UNCLKD);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 3.0f, UI_BUTTON_UNCLKD_TXT);
		}
	}

	bool isClicked() const {
		Vector2 mousePos = GetMousePosition();
		return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height });
	}
};

struct CheckBox {
	const float width = 26.0f;
	const float height = 26.0f;
	const float inWidth = 20.0f;
	const float inHeight = 20.0f;
	float x;
	float y;
	bool active = false;

	CheckBox(int x, int y) : x(x), y(y) {};

	void draw() {
		DrawRectangle(x, y, width, height, UI_CHECKBOX_BG);
		if (active) {
			DrawRectangle(x + 3, y + 3, inWidth, inHeight, UI_CHECKBOX_ACTIVE);
		}
		else {
			DrawRectangle(x + 3, y + 3, inWidth, inHeight, UI_CHECKBOX_INACTIVE);
		}
	}

	void check() {
		Vector2 mousePos = GetMousePosition();
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			if (active) { active = false; }
			else { active = true; }
		}
	}

	bool isChecked() {
		return active;
	}
};

// SIMULATION

struct Body {
	float mass;
	float radius;
	Vector2 velocity;
	Vector2 location;
	Vector2 accumulatedForce = { 0.0f, 0.0f }; // Forces accumulated over a frame from each body
	
	Body(float mass, float radius, Vector2 velocity, Vector2 location) : mass(mass), radius(radius), velocity(velocity), location(location) {};

	void draw() {
		DrawCircle(location.x, location.y, radius, SIM_BDY_COL);

		if (showVectors) {
			Vector2 xVector = { location.x + velocity.x * VECTOR_DRAW_SCALE, location.y };
			Vector2 yVector = { location.x, location.y + velocity.y * VECTOR_DRAW_SCALE };

			if (xVector.x > SIM_WIDTH) {
				DrawLine(location.x, location.y, SIM_WIDTH - 1.0f, xVector.y, RED);
				DrawLine(0.0f, location.y, xVector.x - SIM_WIDTH, xVector.y, RED);
			}
			else if (xVector.x < 0.0f) {
				DrawLine(location.x, location.y, 0.0f, xVector.y, RED);
				DrawLine(SIM_WIDTH - 1.0f, location.y, SIM_WIDTH - (-xVector.x), location.y, RED);
			}
			else {
				DrawLine(location.x, location.y, xVector.x, xVector.y, RED);
			}
			if (yVector.y > SIM_HEIGHT) {
				DrawLine(location.x, location.y, location.x, SIM_HEIGHT - 1.0f, BLUE);
				DrawLine(location.x, 0.0f, location.x, yVector.y - SIM_HEIGHT, BLUE);
			}
			else if (yVector.y < 0.0f) {
				DrawLine(location.x, location.y, location.x, 0.0f, BLUE);
				DrawLine(location.x, SIM_HEIGHT - 1.0f, location.x, SIM_HEIGHT - (-yVector.y), BLUE);
			}
			else {
				DrawLine(location.x, location.y, yVector.x, yVector.y, BLUE);
			}
		}

	}

	void update() {
		// Apply accumulated force to velocity
		velocity.x += accumulatedForce.x / mass;
		velocity.y += accumulatedForce.y / mass;

		location.x += velocity.x;
		location.y += velocity.y;

		// Reset accumulated force for next frame
		accumulatedForce = { 0.0f, 0.0f };

		location.x = fmod(SIM_WIDTH + location.x, SIM_WIDTH);
		location.y = fmod(SIM_HEIGHT + location.y, SIM_HEIGHT);
	}

	void applyForce(Vector2 force) {
		accumulatedForce.x += force.x;
		accumulatedForce.y += force.y;
	}

	static Vector2 calculateGravitationalForce(const Body& body1, const Body& body2) {

		// Precompute raw dx and dy for performance
		float dx = body2.location.x - body1.location.x;
		float dy = body2.location.y - body1.location.y;

		// Adjust for wrap-around while preserving direction.
		if (fabs(dx) > SIM_WIDTH_HALF) {
			dx = (dx > 0) ? dx - SIM_WIDTH : dx + SIM_WIDTH;
		}
		if (fabs(dy) > SIM_HEIGHT_HALF) {
			dy = (dy > 0) ? dy - SIM_HEIGHT : dy + SIM_HEIGHT;
		}
		float distanceSquared = (dx * dx) + (dy * dy); 

		if (distanceSquared < MIN_DISTANCE_SQUARED) return { 0, 0 }; // Avoid division by zero

		float forceMag = G * (body1.mass * body2.mass) / distanceSquared; // Newton's Law of Gravitation, F = (G * (m1 * m2)) / r^2
		float distanceMag = std::sqrt(distanceSquared); // Magnitude of distance vector

		return { (forceMag * dx / distanceMag), (forceMag * dy / distanceMag) };
	}

	static bool checkCollision(const Body& body1, const Body& body2) {

		// Precompute raw dx and dy to avoid redundant calculation
		float rawDx = body2.location.x - body1.location.x;
		float rawDy = body2.location.y - body1.location.y;

		// Find X and Y components of distance between Body 1 and Body 2,
		// This will be the minimum of the screen-space distance or wrap-around distance.
		float dx = std::min(std::fabs(rawDx), SIM_WIDTH - std::fabs(rawDx));
		float dy = std::min(std::fabs(rawDy), SIM_HEIGHT - std::fabs(rawDy));

		float distanceSquared = (dx * dx) + (dy * dy); // Calculate the combined distance of each component squared

		// Precompute sum of radii to avoid redundant calculation
		float radiiSum = body1.radius + body2.radius;

		return distanceSquared <= (radiiSum) * (radiiSum); // Compare this to the minimum collision distance squared.
	}
};

std::vector<Body> bodies;

struct bodySpawner {
	State state = State::DEFAULT;
	//Body* tempBody = nullptr;
	Vector2 startLoc;
	Vector2 velocity;
	float lastMass = 0.0;
	float lastRad = 0.0;


	bodySpawner() {
		startLoc = GetMousePosition();
		velocity = { 0.0f, 0.0f };
	};

	void drawBody() {
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			Vector2 currMouseLoc = GetMousePosition();
			if (currMouseLoc.x < SIM_WIDTH) {
				// If this is the first frame drawing something
				if (state == State::DEFAULT) {

					// Initial Values
					lastRad = 1.0f;
					lastMass = 20000.0f;
					startLoc = currMouseLoc;
					velocity = { 0.0f, 0.0f };

					DrawCircle(startLoc.x, startLoc.y, lastRad, SIM_SPAWN_BDY_COL);

					state = State::DRAWING;
				}
				else if (state == State::DRAWING) {

					// Update Values for tempBody
					lastRad += 0.1f;
					lastMass = (4.0f / 3.0f) * PI * (lastRad * lastRad * lastRad) * 20000.0f; // Mass is based on 10000 kg per pixel volume in a sphere.

					velocity = { (fabs(currMouseLoc.x) - fabs(startLoc.x)) * 0.01f, (fabs(currMouseLoc.y) - fabs(startLoc.y)) * 0.01f }; // Based of vector components of line from startLoc to currMouseLoc

					DrawCircle(startLoc.x, startLoc.y, lastRad, SIM_SPAWN_BDY_COL);
					DrawLine(startLoc.x, startLoc.y, currMouseLoc.x, currMouseLoc.y, SIM_SPAWN_VEL_COL);
				}
			}
		}
		else if (state == State::DRAWING) {
			state = State::DEFAULT;
			bodies.push_back(Body(lastMass, lastRad, velocity, startLoc));
			std::cout << "Mass: " << lastMass << " Velocity: " << sqrt((velocity.x * velocity.x) + (velocity.y * velocity.y)) << std::endl;
		}
	}
};

struct gridSquare {
	float fieldStrength = 0.0f;
	float x;
	float y;
	float width = GRID_SQUARE_SIZE;
	float height = GRID_SQUARE_SIZE;

	gridSquare() {};
	gridSquare(float x, float y) : x(x), y(y) {};
};

struct gravGrid {
	int numSquares = SIM_WIDTH / GRID_SQUARE_SIZE;
	std::vector<std::vector<gridSquare>> gridSquares;

	gravGrid() {
		gridSquares.resize(numSquares);
		for (size_t row = 0; row < numSquares; ++row) {
			gridSquares[row].resize(numSquares);
			for (size_t col = 0; col < numSquares; ++col) {
				gridSquares[row][col] = gridSquare(col * GRID_SQUARE_SIZE, row * GRID_SQUARE_SIZE);
			}
		}
	}

	void updateForces(std::vector<Body>& bodies) {
		for (auto& row : gridSquares) {
			for (auto& square : row) {
				// Reset force to 0 for each frame
				square.fieldStrength = 0.0f;
				for (const auto& body : bodies) { // Get the force from each body

					// Find closest distance to each body
					float rawDx = square.x + GRID_SQUARE_SIZE / 2.0f - body.location.x;
					float rawDy = square.y + GRID_SQUARE_SIZE / 2.0f -body.location.y;

					// Accound for screen wrap-around if shortest distance is not screen-space.
					float dx = std::min(std::fabs(rawDx), SIM_WIDTH - std::fabs(rawDx));
					float dy = std::min(std::fabs(rawDy), SIM_HEIGHT - std::fabs(rawDy));

					float distanceSquared = (dx * dx) + (dy * dy);

					// Gravitational force
					if (distanceSquared > MIN_DISTANCE_SQUARED) {
						square.fieldStrength += G * body.mass / distanceSquared;
					}
				}
			}
		}
	}

	void draw() {
		for (const auto& row : gridSquares) {
			for (const auto& square : row) {
				float scaledStrength = square.fieldStrength * 1e2;
				float normalizedStrength = std::min(1.0f, scaledStrength);
				Color color = ColorFromNormalized({ normalizedStrength , 0.0f ,1.0f / normalizedStrength , normalizedStrength });
				DrawRectangle(square.x, square.y, square.width, square.height, color);
			}
		}
	}

};

int main(void) {
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Gravity");
	SetTargetFPS(60);

	// <--- Init Sim --->
	bodySpawner spawner;
	gravGrid gravityField;

	// <--- Init UI --->
	CheckBox vectorCheck(SIM_WIDTH + 250, 50);
	CheckBox fieldCheck(SIM_WIDTH + 250, 100);
	Button resetSim(SIM_WIDTH + 100, SIM_HEIGHT - 100, 50, 200, "Reset Sim");

	while (!WindowShouldClose()) {

		// <--- Update Sim --->
		if (showField) gravityField.updateForces(bodies);
		showVectors = vectorCheck.isChecked();
		showField = fieldCheck.isChecked();

		// <--- Update UI --->
		vectorCheck.check();
		fieldCheck.check();

		BeginDrawing();
		ClearBackground(SIM_BG_COL);

		spawner.drawBody();

		if (showField) gravityField.draw();
		if (resetSim.isClicked()) bodies.clear();
		

		
		
		

		// Iterate over each unique pair of bodies.
		for (size_t i = 0; i < bodies.size(); i++) {
			// Inner loop starts at i+1 so that each pair is only considered once.
			for (size_t j = i + 1; j < bodies.size(); ) {
				if (Body::checkCollision(bodies[i], bodies[j])) {
					// Collision detected. Merge the two bodies.
					if (bodies[i].mass >= bodies[j].mass) {
						// Merge body j into body i
						float combinedMass = bodies[i].mass + bodies[j].mass;
						Vector2 combinedVelocity = {
							(bodies[i].mass * bodies[i].velocity.x + bodies[j].mass * bodies[j].velocity.x) / combinedMass,
							(bodies[i].mass * bodies[i].velocity.y + bodies[j].mass * bodies[j].velocity.y) / combinedMass
						};
						bodies[i].mass = combinedMass;
						bodies[i].velocity = combinedVelocity;
						bodies[i].radius = cbrt((3.0f * bodies[i].mass) / (4.0f * PI * 10000.0f));

						// Remove body j
						bodies.erase(bodies.begin() + j);
						// Note: Do NOT increment j because the next element has shifted into index j.
					}
					else {
						// Merge body i into body j
						float combinedMass = bodies[i].mass + bodies[j].mass;
						Vector2 combinedVelocity = {
							(bodies[i].mass * bodies[i].velocity.x + bodies[j].mass * bodies[j].velocity.x) / combinedMass,
							(bodies[i].mass * bodies[i].velocity.y + bodies[j].mass * bodies[j].velocity.y) / combinedMass
						};
						bodies[j].mass = combinedMass;
						bodies[j].velocity = combinedVelocity;
						bodies[j].radius = cbrt((3.0f * bodies[j].mass) / (4.0f * PI * 10000.0f));

						// Remove body i and adjust outer loop index.
						bodies.erase(bodies.begin() + i);
						// Since the current i is removed, decrement i (if possible) and break out of the inner loop.
						if (i > 0) i--;
						break;
					}
				}
				else {
					// No collision: apply gravitational force (calculated only once per unique pair).
					Vector2 force = Body::calculateGravitationalForce(bodies[i], bodies[j]);
					bodies[i].applyForce(force);
					bodies[j].applyForce({ -force.x, -force.y });
					// Only increment j when no collision occurred.
					j++;
				}
			}
		}

		// Update and draw each body.
		for (Body& body : bodies) {
			body.update();
			body.draw();
		}

		// <--- Draw UI --->
		
		// Menu Background
		DrawRectangle(SIM_WIDTH, 0.0f, SCREEN_WIDTH - SIM_WIDTH, SCREEN_HEIGHT, UI_MENU_BG);

		// Show framerate
		int fps = GetFPS();
		DrawText(TextFormat("%i FPS", fps), 10, 10, 20, YELLOW);

		// Show Vectors option
		DrawText("Show Vectors", SIM_WIDTH + 50, 50, 25, UI_TEXT);
		vectorCheck.draw();

		// Show Field Option
		DrawText("Show Field", SIM_WIDTH + 50, 100, 25, UI_TEXT);
		fieldCheck.draw();

		// Show Reset Button
		resetSim.DrawButton();

		EndDrawing();
	}

	CloseWindow();
	return 0;
}