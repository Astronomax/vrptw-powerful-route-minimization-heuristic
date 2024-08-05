#include "random_utils.h"

#include "core/random.h"

#include "utils.h"

void
random_subset(int *arr, int n, int k)
{
	for (int i = 0; i < k; i++) {
		int j = (int)pseudo_random_in_range(i, n - 1);
		SWAP(arr[i], arr[j]);
	}
}

void
random_shuffle(int *arr, int n)
{
	random_subset(arr, n, n - 1);
}