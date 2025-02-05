#!/usr/bin/env bash

check_file() {
    local file="$1"
    local line_number=0
    local errors_found=0

    while IFS= read -r line; do
        ((line_number++))
        
        [[ -z "$line" || "$line" =~ ^[[:space:]]*([{}]|//) ]] && continue

        local line_without_comment=$(echo "$line" | sed 's/\/\/.*$//')
        local cleaned_line=$(echo "$line_without_comment" | sed 's/\\"/X/g')
        local quote_count=$(echo "$cleaned_line" | tr -cd '"' | wc -c)

        if [[ $quote_count -eq 2 ]]; then
            if [[ ! "$cleaned_line" =~ ^[[:space:]]*\"[^\"]+\"[[:space:]]*$ ]]; then
                echo "Error in $file:$line_number - Incorrect single-quote format: $line"
                ((errors_found++))
            fi
        elif [[ $quote_count -eq 4 ]]; then
            if [[ ! "$cleaned_line" =~ ^[[:space:]]*\"[^\"]+\"[[:space:]]*\"[^\"]*\"[[:space:]]*$ ]]; then
                echo "Error in $file:$line_number - Incorrect key-value format: $line"
                ((errors_found++))
            fi
        else
            echo "Error in $file:$line_number - Incorrect number of quotes (expected 2 or 4, found $quote_count): $line"
            ((errors_found++))
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
