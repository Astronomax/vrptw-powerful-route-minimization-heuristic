#include "solution_encode.h"

#include <fstream>

#include "dist.h"
#include "utils.h"

void
solution_encode(solution *s, const char *file)
{
	std::string file_string = std::string(file);
	std::ofstream f(file_string);
	for (int i = 0; i < s->n_routes; i++) {
		route *r = s->routes[i];
		double t = -(double)INFINITY;
		for (int j = 0; j < r->size; j++) {
			customer *prev = r->customers[j];
			customer *next = (j + 1 < r->size) ? r->customers[j + 1] : prev;
			t = MAX(prev->e, t);
			f << prev->id << " " << t;
			if (prev != depot_tail(s->routes[i])) {
				f << " ";
				t += prev->s + dist(prev, next);
			}
			prev = next;
		}
		f << "\n";
	}
}
