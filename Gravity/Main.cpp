#include "raylib.h"
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>

// Window Dimensions
const int WIN_WIDTH = 1400;										// Width of Window
const int WIN_HEIGHT = 1000;									// Height of Window

// UI Parameters
const Color UI_MENU_BG = GetColor(0x262626FF);					// Menu Background Color
const Color UI_CHECKBOX_BG = GetColor(0x3A3A3AFF);				// Checkbox Background Color
const Color UI_CHECKBOX_ACTIVE = GetColor(0xBB86FCFF);			// Active checkbox Color
const Color UI_CHECKBOX_INACTIVE = GetColor(0x555555FF);		// Inactive checkbox Color
const Color UI_TEXT = GetColor(0xE0E0E0FF);						// Default Text
const Color UI_BUTTON_UNCLKD = GetColor(0x3A3A3AFF);			// Unclicked Button Color
const Color UI_BUTTON_UNCLKD_TXT = GetColor(0xE0E0E0FF);		// Unclicked Button Text
const Color UI_BUTTON_HVR = GetColor(0xBDBDBDFF);				// Hovered Button Color
const Color UI_BUTTON_HVR_TXT = GetColor(0x000000FF);			// Hovered Button Text
const Color UI_BUTTON_CLKD = GetColor(0x9E9E9EFF);				// Clicked Button Color
const Color UI_BUTTON_CLKD_TXT = GetColor(0xFFFFFFFF);			// Clicked Button Text

// Sim Parameters
const int SIM_WIDTH = WIN_WIDTH - 400;							// Width of simulation space
const int SIM_HEIGHT = WIN_HEIGHT;								// Height of simulation space
const float SIM_WIDTH_HALF = SIM_WIDTH / 2.0f;					// Half of simulation width
const float SIM_HEIGHT_HALF = SIM_HEIGHT / 2.0f;				// Half of simulation height 
const float G = 6.67430e-8;										// Gravitational Constant (Modified to fit simulation scale)
const float MIN_DISTANCE_SQUARED = 0.1f;						// Threshold to avoid division by zero in force calculations.
const float VECTOR_DRAW_SCALE = 50.0f;							// Scalar to draw vectors at visible lengths
const float FIELD_CELL_SIZE = 5.0f;								// Size of Gravity Field cell (Low values reduce performance)
const Color SIM_BG_COL = GetColor(0x020202FF);					// Sim Space background color
const Color SIM_BDY_COL = GetColor(0xC9C9C9FF);					// Sim Space body color
const Color SIM_SPAWN_BDY_COL = GetColor(0xB09C02FF);			// Spawn body color
const Color SIM_SPAWN_VEL_COL = GetColor(0xB09C02FF);			// Spawn vector color
bool showVectors = false;										// Control vector visibility
bool showField = false;											// Control gravity field visibility
int fieldScalar = 1;											// Gravity field visualization sensitivity
enum State {													// State to track if user is spawning a body
	DEFAULT,
	SPAWNING
};											

// <--- UI --->

// Defines Button UI Element
struct Button {
	const std::string message;
	const float x;
	const float y;
	const float width;
	const float height;	

	// Button Constructor
	Button(float x, float y, float h, float w, const std::string& m) :
		x(x),
		y(y),
		height(h),
		width(w),
		message(m)
	{}

	// Draw Button using raylib primitives
	void DrawButton() const {
		int textWidth = MeasureText(message.c_str(), height / 2.0f);
		Vector2 mousePos = GetMousePosition();
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			DrawRectangle(x, y, width, height, UI_BUTTON_CLKD);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 2.0f, UI_BUTTON_CLKD_TXT);
		}
		else if (CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			DrawRectangle(x, y, width, height, UI_BUTTON_HVR);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 2.0f, UI_BUTTON_HVR_TXT);
		}
		else {
			DrawRectangle(x, y, width, height, UI_BUTTON_UNCLKD);
			DrawText(message.c_str(), x + (width - textWidth) / 2.0f, y + height / 3.0f, height / 2.0f, UI_BUTTON_UNCLKD_TXT);
		}
	}

	// Return if button is clicked
	bool isClicked() const {
		Vector2 mousePos = GetMousePosition();
		return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height });
	}
};

// Defines Checkbox UI Element
struct CheckBox {
	const float width = 26.0f;
	const float height = 26.0f;
	const float inWidth = 20.0f;
	const float inHeight = 20.0f;
	float x;
	float y;
	bool active = false;

	// Constructor
	CheckBox(int x, int y) : x(x), y(y) {};

	// Draw using raylib primitives
	void draw() {
		DrawRectangle(x, y, width, height, UI_CHECKBOX_BG);
		if (active) {
			DrawRectangle(x + 3, y + 3, inWidth, inHeight, UI_CHECKBOX_ACTIVE);
		}
		else {
			DrawRectangle(x + 3, y + 3, inWidth, inHeight, UI_CHECKBOX_INACTIVE);
		}
	}

	// Updates activity state of checkbox
	void check() {
		Vector2 mousePos = GetMousePosition();
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, { x, y, width, height })) {
			if (active) { active = false; }
			else { active = true; }
		}
	}

	// Returns checkbox activity state
	bool isChecked() {
		return active;
	}
};

// <--- SIMULATION --->

// Defines Body Simulation Element
struct Body {
	float mass;
	float radius;
	Vector2 velocity;
	Vector2 location;
	Vector2 accumulatedForce = { 0.0f, 0.0f }; // Forces accumulated over one frame from each body
	
	// Constructor
	Body(float mass, float radius, Vector2 velocity, Vector2 location) : mass(mass), radius(radius), velocity(velocity), location(location) {};

	// Draws body using raylib primitives
	void draw() {

		// Draw the body
		DrawCircle(location.x, location.y, radius, SIM_BDY_COL);

		if (showVectors) {

			// Calculate endpoint of each component velocity vector, multiplied by a scalar for visibility.
			Vector2 xVector = { location.x + velocity.x * VECTOR_DRAW_SCALE, location.y };
			Vector2 yVector = { location.x, location.y + velocity.y * VECTOR_DRAW_SCALE };

			// Draw each component velocity vector and the velocity vector
			DrawLine(location.x, location.y, xVector.x, xVector.y, RED);
			DrawLine(location.x, location.y, yVector.x, yVector.y, BLUE);
			DrawLine(location.x, location.y, xVector.x, yVector.y, WHITE);
		}
	}

	// Apply forces to body
	void applyForce() {
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

	// Accumulate forces over one frame
	void accumulateForce(Vector2 force) {
		accumulatedForce.x += force.x;
		accumulatedForce.y += force.y;
	}

	// Calculate forces between two bodies
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

	// Check if two bodies are colliding
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

// Defines Body Spawing Behavior and UI
struct bodySpawner {
	State state = State::DEFAULT; // Determines if we create a new body or grow the current new body
	Vector2 spawnPos;
	Vector2 velocity;
	float spawnMass = 0.0;
	float spawnRad = 0.0;

	// Constructor
	bodySpawner() {
		spawnPos = GetMousePosition();
		velocity = { 0.0f, 0.0f };
	};

	// Draw spawning elements and create new body
	void drawBody(std::vector<Body>& bodies) {

		// Only enter spawner if user is clicking
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			Vector2 currMouseLoc = GetMousePosition();

			// Ensure mouse is within simulation bounds
			if (currMouseLoc.x < SIM_WIDTH) {

				// If not spawning new body already, start spawning
				if (state == State::DEFAULT) {

					// Initial Values
					spawnRad = 1.0f;
					spawnMass = 20000.0f; // Mass per cubic pixel unit
					spawnPos = currMouseLoc;
					velocity = { 0.0f, 0.0f };

					DrawCircle(spawnPos.x, spawnPos.y, spawnRad, SIM_SPAWN_BDY_COL);

					state = State::SPAWNING;
				}
				// If already spawning new body
				else if (state == State::SPAWNING) { 

					// Update Values for new body
					spawnRad += 0.1f;
					spawnMass = (4.0f / 3.0f) * PI * (spawnRad * spawnRad * spawnRad) * 20000.0f; // Mass is based on 20000 kg per pixel volume in a sphere.

					velocity = { (fabs(currMouseLoc.x) - fabs(spawnPos.x)) * 0.01f, (fabs(currMouseLoc.y) - fabs(spawnPos.y)) * 0.01f }; // Based on vector components of line from startPos to currMouseLoc

					DrawCircle(spawnPos.x, spawnPos.y, spawnRad, SIM_SPAWN_BDY_COL);
					DrawLine(spawnPos.x, spawnPos.y, currMouseLoc.x, currMouseLoc.y, SIM_SPAWN_VEL_COL);
				}
			}
		}
		// If user is not clicking and was spawning last frame
		else if (state == State::SPAWNING) { 

			state = State::DEFAULT; // Set state back to default
			bodies.push_back(Body(spawnMass, spawnRad, velocity, spawnPos)); // Add new body to bodies
			
		}
	}
};

// Defines gravity field cell
struct fieldCell {
	float fieldStrength = 0.0f;
	float x;
	float y;
	float width = FIELD_CELL_SIZE;
	float height = FIELD_CELL_SIZE;

	fieldCell() {};
	fieldCell(float x, float y) : x(x), y(y) {};
};

// Defines gravity field visualization
struct fieldGrid {

	int numCells = SIM_WIDTH / FIELD_CELL_SIZE; // Amount of cells that will fit in sim space
	std::vector<std::vector<fieldCell>> fieldCells; // Vector holding all cells

	// Constructer for grid
	fieldGrid() {
		fieldCells.resize(numCells);
		for (size_t row = 0; row < numCells; ++row) {
			fieldCells[row].resize(numCells);
			for (size_t col = 0; col < numCells; ++col) {
				fieldCells[row][col] = fieldCell(col * FIELD_CELL_SIZE, row * FIELD_CELL_SIZE);
			}
		}
	}

	// Find strength of gravitational acceleration in the center of the field cell
	void updateForces(std::vector<Body>& bodies) {
		for (auto& row : fieldCells) {
			for (auto& square : row) {
				// Reset force to 0 for each frame
				square.fieldStrength = 0.0f;
				for (const auto& body : bodies) { // Get the force from each body

					// Find closest distance to each body
					float rawDx = square.x + FIELD_CELL_SIZE / 2.0f - body.location.x;
					float rawDy = square.y + FIELD_CELL_SIZE / 2.0f -body.location.y;

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

	// Draw the gravity field
	void draw() {
		for (const auto& row : fieldCells) {
			for (const auto& square : row) {
				float scaledStrength = square.fieldStrength * fieldScalar * fieldScalar;
				float normalizedStrength = std::clamp(scaledStrength, 0.01f, 1.0f);
				
				Color color = ColorFromNormalized({ normalizedStrength, 0.0f , 1.0f / normalizedStrength, 1.0f });
				DrawRectangle(square.x, square.y, square.width, square.height, getFieldColor(normalizedStrength));
			}
		}
	}

	// Generate heat map colors based on normalized value
	Color getFieldColor(float norm) {

		// Because the normalized force follows the inverse square law, halving the distance between bodies increases the force by a factor of 4
		// so we visualize based on the square root of this force, meaning that force grows linearly with distance. This creates a more even
		// distribution of values for the heatmap.
		norm = std::cbrtf(norm);

		if (norm <= 0.2f) {
			float tmp = norm * 5;  
			return ColorFromNormalized({ 0.0f, 0.0f, tmp, 1.0f }); // black to blue
		}
		else if (norm <= 0.4f) {
			float tmp = (norm - 0.2f) * 5; 
			return ColorFromNormalized({ 0.0f, tmp, 1.0f, 1.0f }); // blue to cyan
		}
		else if (norm <= 0.6f) {
			float tmp = (norm - 0.4f) * 5;
			return ColorFromNormalized({ 0.0f, 1.0f, 1.0f - tmp, 1.0f }); // cyan to green
		}
		else if (norm <= 0.8f) {
			float tmp = (norm - 0.6f) * 5;
			return ColorFromNormalized({ tmp, 1.0f, 0.0f, 1.0f }); // green to yellow
		}
		else if (norm <= 0.95f) {
			float tmp = (norm - 0.8f) / 0.15f;
			return ColorFromNormalized({ 1.0f, 1.0f - tmp, 0.0f, 1.0f }); // yellow to red
		}
		else {
			float tmp = (norm - 0.95f) / 0.05f;
			return ColorFromNormalized({ 1.0f, 0.0f, tmp, 1.0f }); // red to pink
		}
	}
};

int main(void) {
	// Initialize simulation window
	InitWindow(WIN_WIDTH, WIN_HEIGHT, "Gravity Toy");
	SetTargetFPS(60);

	// Initialize Sim Elements
	bodySpawner spawner;
	fieldGrid gravityField;
	std::vector<Body> bodies; // Vector Containing all existing bodies

	// Initialize UI Elements
	CheckBox vectorCheck(SIM_WIDTH + 250, 50);
	CheckBox fieldCheck(SIM_WIDTH + 250, 100);
	Button minusFieldStrength(SIM_WIDTH + 50, 200, 40, 40, "-");
	Button plusFieldStrength(SIM_WIDTH + 300, 200, 40, 40, "+");
	Button resetSim(SIM_WIDTH + 100, SIM_HEIGHT - 100, 50, 200, "Reset Sim");

	// Simulation Loop
	while (!WindowShouldClose()) {

		// Update Sim
		if (showField) gravityField.updateForces(bodies);
		showVectors = vectorCheck.isChecked();
		showField = fieldCheck.isChecked();

		// Update UI
		vectorCheck.check();
		fieldCheck.check();

		// Listen for field scalar adjustment
		if (minusFieldStrength.isClicked() && fieldScalar > 1) fieldScalar -= 1;
		if (plusFieldStrength.isClicked()) fieldScalar += 1;

		BeginDrawing();
		ClearBackground(SIM_BG_COL);

		// Draw Vectors or Gravity Field
		if (showField) gravityField.draw();
		if (resetSim.isClicked()) bodies.clear();
		
		// Iterate over each unique pair of bodies
		for (size_t i = 0; i < bodies.size(); i++) {
			
			for (size_t j = i + 1; j < bodies.size(); ) {

				// Handle Collisions
				if (Body::checkCollision(bodies[i], bodies[j])) {
					
					if (bodies[i].mass >= bodies[j].mass) { // Merge body j into body i
						
						// Calculate attributes of merged body
						float combinedMass = bodies[i].mass + bodies[j].mass; 
						Vector2 combinedVelocity = { 
							(bodies[i].mass * bodies[i].velocity.x + bodies[j].mass * bodies[j].velocity.x) / combinedMass,
							(bodies[i].mass * bodies[i].velocity.y + bodies[j].mass * bodies[j].velocity.y) / combinedMass
						};

						// Assign attributes
						bodies[i].mass = combinedMass;
						bodies[i].velocity = combinedVelocity;
						bodies[i].radius = cbrt((3.0f * bodies[i].mass) / (4.0f * PI * 10000.0f));

						// Remove old body
						bodies.erase(bodies.begin() + j);
					}
					else {	// Merge body i into body j
						
						// Calculate attributes of merged body
						float combinedMass = bodies[i].mass + bodies[j].mass;
						Vector2 combinedVelocity = {
							(bodies[i].mass * bodies[i].velocity.x + bodies[j].mass * bodies[j].velocity.x) / combinedMass,
							(bodies[i].mass * bodies[i].velocity.y + bodies[j].mass * bodies[j].velocity.y) / combinedMass
						};

						// Assign attributes
						bodies[j].mass = combinedMass;
						bodies[j].velocity = combinedVelocity;
						bodies[j].radius = cbrt((3.0f * bodies[j].mass) / (4.0f * PI * 10000.0f));

						// Remove old body
						bodies.erase(bodies.begin() + i);

						// Since the current i is removed, decrement i (if possible) and break out of the inner loop.
						if (i > 0) i--;
						break;
					}
				}
				else { // No collision: apply gravitational force.
					
					Vector2 force = Body::calculateGravitationalForce(bodies[i], bodies[j]);
					bodies[i].accumulateForce(force);
					bodies[j].accumulateForce({ -force.x, -force.y }); // equal and opposite force
					j++;
				}
			}
		}

		// Update and draw each body.
		for (Body& body : bodies) {
			body.applyForce();
			body.draw();
		}

		// Draw Body Spawning
		spawner.drawBody(bodies);

		// <--- Draw UI --->
		
		// Menu Background
		DrawRectangle(SIM_WIDTH, 0.0f, WIN_WIDTH - SIM_WIDTH, WIN_HEIGHT, UI_MENU_BG);

		// Show framerate
		int fps = GetFPS();
		DrawText(TextFormat("%i FPS", fps), 10, 10, 20, YELLOW);

		// Show body count
		if (bodies.size() == 1) { DrawText(TextFormat("%i BODY", bodies.size()), 10, 30, 20, GREEN); }
		else { DrawText(TextFormat("%i BODIES", bodies.size()), 10, 30, 20, GREEN); }
			

		// Show Vectors option
		DrawText("Show Vectors", SIM_WIDTH + 50, 50, 25, UI_TEXT);
		vectorCheck.draw();

		// Show Field Option
		DrawText("Show Field", SIM_WIDTH + 50, 100, 25, UI_TEXT);
		fieldCheck.draw();

		// Show Field Strength
		DrawText("Field Strength Scalar", SIM_WIDTH + 50, 150, 25, UI_TEXT);
		minusFieldStrength.DrawButton();
		DrawText(TextFormat("%i ^ 2", fieldScalar), SIM_WIDTH + 175, 200, 25, UI_TEXT);
		plusFieldStrength.DrawButton();

		// Show Reset Button
		resetSim.DrawButton();

		EndDrawing();
	}

	CloseWindow();
	return 0;
}