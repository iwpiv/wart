(defun match(a b)
  (or (equal a b)
      (equal b '_)
      (and (consp a) (consp b)
           (match (car a) (car b))
           (match (cdr a) (cdr b)))))
