#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "cvector.h"
#include "pcg_variants.h"
#include "entropy.h"

#include "phtree.h"

/*
 * this demo hides (ish) direct interaction with the phtree
 * tree insertions and queries are wrapped to keep interaction with the tree
 * 	consistant with the data that is used everywhere outside of the tree
 *
 * this seems like completely unnecessary complexity for this demo
 * 	but, whatever, we're all learning here :)
 */

typedef phtree_entry_t cell_t;

typedef struct
{
	Vector2 min;
	Vector2 max;
	phtree_window_query_t* phtree_query;
} demo_query_t;

int screen_width = 1024;
int screen_height = 1024;

/*
 * convert a raylib Vector2 to a phtree point
 *
 * phtree entries are cells which represent 64x64 pixel squares
 * 	we divide the input point by 64
 * 	and truncate the fractional part
 * 		when the results of the division are passed in to point_set as key_type_signed_t
 * 			(signed integer)
 * 	the divided, truncated, input point is the address of the cell the point will be put in
 * 		a spatial hash
 */
phtree_point_t world_point_to_tree_point (Vector2 point)
{
	phtree_point_t out;

	point_set (&out, point.x / 64, point.y / 64);

	return out;
}

/*
 * wrapping tree insertion with this function
 * since the rendered/selectable points are not being stored directly in the phtree
 */
void tree_insert_point (phtree_t* tree, Vector2* point, int point_id)
{
	phtree_point_t tree_point = world_point_to_tree_point (*point);

	tree_insert (tree, &tree_point, point_id);
}

demo_query_t* query_create (Vector2 min, Vector2 max)
{
	demo_query_t* new_query = malloc (sizeof (*new_query));

	new_query->min = min;
	new_query->max = max;
	new_query->phtree_query = window_query_create (world_point_to_tree_point (min), world_point_to_tree_point (max));

	return new_query;
}

void query_update (demo_query_t* query, Vector2 min, Vector2 max)
{
	window_query_clear (query->phtree_query);
	query->min = min;
	query->max = max;
	query->phtree_query->min = world_point_to_tree_point (min);
	query->phtree_query->max = world_point_to_tree_point (max);
}

static inline void draw_point (Vector2* point, Color color)
{
	DrawRectangle (point->x - 2, point->y - 2, 4, 4, color);
}

void query_window (phtree_t* tree, demo_query_t* query)
{
	tree_query_window (tree, query->phtree_query);
}

void query_clear (demo_query_t* query)
{
	query->min = query->max = (Vector2) {0.0f, 0.0f};
	window_query_clear (query->phtree_query);
}

void query_free (demo_query_t* query)
{
	window_query_free (query->phtree_query);

	free (query);
}

cvector (cell_t*) query_get_cells (demo_query_t* query)
{
	return query->phtree_query->entries;
}

int main ()
{
	uint64_t pcg_seed;
	uint64_t pcg_sequence;
	entropy_getbytes (&pcg_seed, sizeof (pcg_seed));
	entropy_getbytes (&pcg_sequence, sizeof (pcg_sequence));
	pcg32_srandom (pcg_seed, pcg_sequence);

	SetTraceLogLevel (LOG_WARNING);
	SetConfigFlags (FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow (screen_width, screen_height, "phtree demo");
	SetTargetFPS (60);

	Font font = LoadFontEx ("resources/fonts/dejavu-mono-2.37/ttf/DejaVuSansMono.ttf", 32, NULL, 0);

	phtree_t* tree = tree_create ();

	cvector (Vector2) points = NULL;
	cvector_init (points, 500, NULL);

	for (int iter = 0; iter < 500; iter++)
	{
		Vector2 new_point;

		new_point.x = pcg32_boundedrand (1024);
		new_point.y = pcg32_boundedrand (1024);

		cvector_push_back (points, new_point);
		tree_insert_point (tree, &new_point, iter);
	}

	Color selection_color =
	{
		.r = 147,
		.g = 171,
		.b = 147,
		.a = 255
	};

	bool box_select_active = false;
	Vector2 box_select_min = {0.0f, 0.0f};
	Vector2 box_select_max = {0.0f, 0.0f};
	demo_query_t* box_query = query_create (box_select_min, box_select_max);
	Vector2 box_select_origin = {0.0f, 0.0f};
	Rectangle box_rectangle = {0};

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
			query_update (box_query, box_select_min, box_select_max);

			query_window (tree, box_query);

			box_rectangle = (Rectangle) {box_select_min.x, box_select_min.y, box_select_max.x - box_select_min.x, box_select_max.y - box_select_min.y};
		}

		if (IsKeyPressed (KEY_SPACE))
		{
			query_clear (box_query);
			box_select_active = false;
		}

		BeginDrawing ();
		{
			ClearBackground (BLACK);

			for (int iter = 0; iter < cvector_size (points); iter++)
			{
				draw_point (&points[iter], WHITE);
			}

			// this pointer is _ONLY_ for iterating
			// 	dont make changes to the vector (add/remove/etc...)
			cvector (cell_t*) cells = query_get_cells (box_query);

			if (cvector_size (cells) > 0)
			{
				for (int iter = 0; iter < cvector_size (cells); iter++)
				{
					int value_x = key_to_value (cells[iter]->point.values[0]);
					int value_y = key_to_value (cells[iter]->point.values[1]);

					DrawRectangle (value_x * 64, value_y * 64, 64, 64, selection_color);

					char entry_id[16] = {0};

					sprintf (entry_id, "{%i,%i}", value_x, value_y);
					DrawTextEx (font, entry_id, (Vector2) {value_x * 64, value_y * 64}, 16, 1.0f, BLACK);

					for (int jter = 0; jter < cvector_size (cells[iter]->elements); jter++)
					{
						Vector2* point = &points[cells[iter]->elements[jter]];

						if (CheckCollisionPointRec ((Vector2) {point->x, point->y}, box_rectangle))
						{
							draw_point (point, RED);
						}
						else
						{
							draw_point (point, WHITE);
						}
					}
				}
			}

			if (box_select_active)
			{
				DrawRectangleLines (box_select_min.x, box_select_min.y, box_select_max.x - box_select_min.x, box_select_max.y - box_select_min.y, BLUE);
			}
		}
		EndDrawing ();
	}

	cvector_free (points);

	query_free (box_query);

	tree_clear (tree);
	free (tree);

	UnloadFont (font);

	CloseWindow ();

	return 0;
}
