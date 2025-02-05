#!/usr/bin/env bash

check_file() {
    local file="$1"
    local line_number=0
    local errors_found=0

    while IFS= read -r line; do
        ((line_number++))
        
        [[ -z "$line" || "$line" =~ ^[[:space:]]*([{}]|//) ]] && continue

        local cleaned_line=$(echo "$line" | sed 's/\\"/X/g')
        local quote_count=$(echo "$cleaned_line" | tr -cd '"' | wc -c)

        if [[ $quote_count -ne 4 ]]; then
            echo "Error in $file:$line_number - Incorrect number of quotes (expected 4, found $quote_count): $line"
            ((errors_found++))
        else
            if [[ ! "$cleaned_line" =~ ^[[:space:]]*\"[^\"]+\"[[:space:]]*\"[^\"]*\"[[:space:]]*$ ]]; then
                echo "Error in $file:$line_number - Incorrect key-value format: $line"
                ((errors_found++))
            fi
        fi
    done < "$file"

    return $errors_found
}

errors_total=0
exit_code=0

while IFS= read -r -d '' file; do
    check_file "$file"
    errors_total=$((errors_total + $?))
done < <(find ./translations -type f -name "*.txt" -print0)

if [ $errors_total -eq 0 ]; then
    echo "No errors found. All strings are properly formatted."
else
    echo "Total errors found: $errors_total"
    exit_code=1
fi

exit "$exit_code"
