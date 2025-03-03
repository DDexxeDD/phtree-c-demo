#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "cvector.h"
#include "pcg.h"
#include "phtree32_2d.h"


/*
 * this demo hides (ish) direct interaction with the phtree
 * tree insertions and queries are wrapped to keep interaction with the tree
 * 	consistant with the data that is used everywhere outside of the tree
 *
 * this seems like completely unnecessary complexity for this demo
 * 	but, whatever, we're all learning here :)
 */

typedef struct
{
	int id;
	Vector2 position;
} point_t;

typedef struct
{
	int x;
	int y;
	cvector (int) points;
} cell_t;

typedef struct
{
	Vector2 min;
	Vector2 max;
	ph2_t* tree;
	ph2_query_t query;
	// vector to store queried cells in
	cvector (cell_t*) cells;
} demo_query_t;

int screen_width = 1024;
int screen_height = 1024;

/*
 * convert a raylib Vector2 to a phtree point
 *
 * phtree entries are cells which represent 64x64 pixel squares
 * 	we divide the input point by 64
 * 	and floor the fractional part
 * 	the divided, floored, input point is the address of the cell the point will be put in
 * 		a spatial hash
 */

/*
 * cell creation function for the phtree
 */
void* cell_create (void* input)
{
	cell_t* new_cell = calloc (1, sizeof (*new_cell));

	if (!new_cell)
	{
		return NULL;
	}

	cvector_init (new_cell->points, 2, NULL);

	Vector2* vector = input;

	new_cell->x = floorf (vector->x / 64.0f);
	new_cell->y = floorf (vector->y / 64.0f);

	return new_cell;
}

/*
 * cell destruction function for the phtree
 */
void cell_destroy (void* cell_in)
{
	cell_t* cell = cell_in;

	cvector_free (cell->points);
	free (cell);
}

/*
 * convert a float to a cell key in the phtree
 *
 * phtree entries are cells which represent 64x64 pixel squares
 * 	we divide the input point by 64
 * 	and floor the fractional part
 * 	the divided, truncated, input value is part of the address
 * 		of the cell the point will be put in
 */
phtree_key_t float_to_key (void* input)
{
	// cells are 64 pixels wide/tall
	// 	so we divide by 64
	// 	and floor so that negative numbers end up in the correct cell
	int value = floorf (*(float*) input / 64.0f);
	phtree_key_t out = 0;

	memcpy (&out, &value, sizeof (phtree_key_t));
	// TODO
	// 	PHTREE_SIGN_BIT
	out ^= (PHTREE_KEY_ONE << (PHTREE_BIT_WIDTH - 1));  // flip sign bit

	return out;
}

/*
 * convert a Vector2 to a point in the phtree
 */
void vector2_to_tree (ph2_t* tree, ph2_point_t* out, void* input)
{
	Vector2* vector = input;
	// float_to_key will be called on vector->x/y inside of ph2_point_set
	// 	because we set tree->convert_to_key to float_to_key
	ph2_point_set (tree, out, &vector->x, &vector->y);
}

void tree_insert_point (ph2_t* tree, point_t* point)
{
	// the tree only cares about a point's position
	cell_t* cell = ph2_insert (tree, &point->position);

	cvector_push_back (cell->points, point->id);
}

void query_initialize (demo_query_t* query, ph2_t* tree, Vector2* min, Vector2* max, phtree_iteration_function_t function)
{
	query->min = *min;
	query->max = *max;
	query->tree = tree;

	query->cells = NULL;
	cvector_init (query->cells, 10, NULL);

	ph2_query_set (tree, &query->query, min, max, function);
}

void query_update_bounds (demo_query_t* query, Vector2* min, Vector2* max)
{
	query->min = *min;
	query->max = *max;

	cvector_clear (query->cells);

	ph2_query_set (query->tree, &query->query, min, max, query->query.function);
}

static inline void draw_point (Vector2* point, Color color)
{
	DrawRectangle (point->x - 2, point->y - 2, 4, 4, color);
}

void query_clear (demo_query_t* query)
{
	query->min = query->max = (Vector2) {0.0f, 0.0f};
	ph2_query_set (query->tree, &query->query, &query->min, &query->max, query->query.function);
	cvector_clear (query->cells);
}

void query_release (demo_query_t* query)
{
	ph2_query_clear (&query->query);
	cvector_free (query->cells);
}

void query_cache_cells (void* cell, void* demo_query_in)
{
	demo_query_t* query = demo_query_in;

	cvector_push_back (query->cells, cell);
}

void query_run (demo_query_t* query)
{
	ph2_query (query->tree, &query->query, query);
}

// FIXME
// 	something is broken somewhere :D
int main ()
{
	pcg32_entropy_seed ();

	SetTraceLogLevel (LOG_WARNING);
	SetConfigFlags (FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow (screen_width, screen_height, "phtree demo");
	SetTargetFPS (60);

	Font font = LoadFontEx ("resources/fonts/dejavu-mono-2.37/ttf/DejaVuSansMono.ttf", 32, NULL, 0);

	ph2_t* tree = ph2_create (cell_create, cell_destroy, float_to_key, vector2_to_tree, NULL);

	cvector (point_t) points = NULL;
	cvector_init (points, 500, NULL);

	for (int iter = 0; iter < 500; iter++)
	{
		point_t new_point;

		new_point.position.x = pcg32_boundedrand (1024);
		new_point.position.y = pcg32_boundedrand (1024);
		new_point.id = iter;

		cvector_push_back (points, new_point);
		tree_insert_point (tree, &new_point);
	}

	Color selection_color =
	{
		.r = 147,
		.g = 171,
		.b = 147,
		.a = 255
	};

	bool show_help = true;
	bool box_select_active = false;
	Vector2 box_select_min = {0.0f, 0.0f};
	Vector2 box_select_max = {0.0f, 0.0f};
	Vector2 box_select_origin = {0.0f, 0.0f};
	Rectangle box_rectangle = {0};
	demo_query_t box_query;

	query_initialize (&box_query, tree, &box_select_min, &box_select_max, query_cache_cells);

	while (!WindowShouldClose ())
	{
		Vector2 mouse_position = GetMousePosition ();

		if (IsMouseButtonPressed (MOUSE_BUTTON_LEFT))
		{
			box_select_active = true;
			box_select_min = mouse_position;
			box_select_max = mouse_position;
			box_select_origin = mouse_position;
		}

		if (IsMouseButtonDown (MOUSE_BUTTON_LEFT))
		{
			if (mouse_position.x <= box_select_origin.x)
			{
				box_select_min.x = mouse_position.x;
				box_select_max.x = box_select_origin.x;
			}
			else if (mouse_position.x > box_select_origin.x)
			{
				box_select_max.x = mouse_position.x;
				box_select_min.x = box_select_origin.x;
			}

			if (mouse_position.y <= box_select_origin.y)
			{
				box_select_min.y = mouse_position.y;
				box_select_max.y = box_select_origin.y;
			}
			else if (mouse_position.y > box_select_origin.y)
			{
				box_select_max.y = mouse_position.y;
				box_select_min.y = box_select_origin.y;
			}
		}

		if (IsMouseButtonReleased (MOUSE_BUTTON_LEFT))
		{
			query_update_bounds (&box_query, &box_select_min, &box_select_max);
			query_run (&box_query);

			box_rectangle = (Rectangle) {box_select_min.x, box_select_min.y, box_select_max.x - box_select_min.x, box_select_max.y - box_select_min.y};
		}

		if (IsKeyPressed (KEY_SPACE))
		{
			query_clear (&box_query);
			box_select_active = false;
		}

		if (IsKeyPressed (KEY_H))
		{
			show_help = !show_help;
		}

		BeginDrawing ();
		{
			ClearBackground (BLACK);

			for (int iter = 0; iter < cvector_size (points); iter++)
			{
				draw_point (&points[iter].position, WHITE);
			}

			if (cvector_size (box_query.cells) > 0)
			{
				for (int iter = 0; iter < cvector_size (box_query.cells); iter++)
				{
					int x = box_query.cells[iter]->x;
					int y = box_query.cells[iter]->y;

					DrawRectangle (x * 64, y * 64, 64, 64, selection_color);

					char entry_id[16] = {0};

					sprintf (entry_id, "{%i,%i}", x, y);
					DrawTextEx (font, entry_id, (Vector2) {x * 64, y * 64}, 16, 1.0f, BLACK);

					for (int jter = 0; jter < cvector_size (box_query.cells[iter]->points); jter++)
					{
						point_t* point = &points[box_query.cells[iter]->points[jter]];

						if (CheckCollisionPointRec ((Vector2) {point->position.x, point->position.y}, box_rectangle))
						{
							draw_point (&point->position, RED);
						}
						else
						{
							draw_point (&point->position, WHITE);
						}
					}
				}
			}

			if (box_select_active)
			{
				DrawRectangleLines (box_select_min.x, box_select_min.y, box_select_max.x - box_select_min.x, box_select_max.y - box_select_min.y, BLUE);
			}

			if (show_help)
			{
				DrawRectangle (8, 8, 339, 72, WHITE);
				DrawRectangle (10, 10, 335, 68, BLACK);
				DrawTextEx (font, "Press 'h' to hide help", (Vector2) {12.0f, 12.0f}, 16, 1, GREEN);
				DrawTextEx (font, "Click and drag mouse to select points", (Vector2) {12.0f, 35.0f}, 16, 1, GREEN); 
				DrawTextEx (font, "Press 'space' to clear selection", (Vector2) {12.0f, 58.0f}, 16, 1, GREEN);
			}
			else
			{
				DrawRectangle (8, 8, 169, 24, WHITE);
				DrawRectangle (10, 10, 165, 20, BLACK);
				DrawTextEx (font, "Press 'h' for help", (Vector2) {12.0f, 12.0f}, 16, 1, GREEN);
			}
		}
		EndDrawing ();
	}

	cvector_free (points);
	query_release (&box_query);
	ph2_free (tree);

	UnloadFont (font);

	CloseWindow ();

	return 0;
}
