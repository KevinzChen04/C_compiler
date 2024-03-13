void main() {
	int64[] arr
	arr <- read_numbers()
	bubble_sort(arr)
	print_on_separate_lines(arr)
}

void bubble_sort(int64[] arr) {
	int64 end
	end <- length arr 0
	end <- end - 1

	while (end > 0) :outer_body :conclusion
	{
		:outer_body
		int64 i
		i <- 0

		while (i < end) :inner_body :l0
		{
			:inner_body
			int64 left_val
			left_val <- arr[i]
			int64 j
			j <- i + 1
			int64 right_val
			right_val <- arr[j]
			if (left_val > right_val) :swap :l1
			{
				:swap
				arr[j] <- left_val
				arr[i] <- right_val
			}

			:l1
			i <- j
			continue
		}

		:l0
		end <- end - 1
		continue
	}

	:conclusion
	return
}

void print_on_separate_lines(int64[] arr) {
	int64 len
	len <- length arr 0
	int64 i
	i <- 0

	while (i < len) :body :conclusion
	{
		:body
		int64 num
		num <- arr[i]
		print(num)
		i <- i + 1
		continue
	}

	:conclusion
	return
}

int64[] read_numbers() {
 	int64 len
 	len <- input()
	int64[] result
 	result <- new Array(len)
	int64 i
	i <- 0
	while (i < len) :body :exit
	{
		:body
		int64 num
		num <- input()
		result[i] <- num
		i <- i + 1
		continue
	}
	:exit
	return result
}
