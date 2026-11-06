-- Executed by Database::initializeSchema inside one transaction.
-- This migration is only for an empty, dedicated database.
CREATE TABLE public.habits (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT habits_valid_name CHECK (
        char_length(name) BETWEEN 1 AND 100
        AND name = btrim(name)
        AND name !~ '^[[:space:]]*$'
        AND name !~ E'[\r\n]'
    )
);

CREATE TABLE public.habit_completions (
    habit_id BIGINT NOT NULL REFERENCES public.habits(id) ON DELETE CASCADE,
    completed_on DATE NOT NULL,
    PRIMARY KEY (habit_id, completed_on),
    CONSTRAINT completions_calendar_range CHECK (
        completed_on BETWEEN DATE '0001-01-01' AND DATE '9999-12-31'
    )
);
CREATE INDEX habit_completions_by_date ON public.habit_completions(completed_on);

CREATE TABLE public.schema_version (
    singleton BOOLEAN PRIMARY KEY DEFAULT TRUE CHECK (singleton),
    version INTEGER NOT NULL CHECK (version > 0)
);
INSERT INTO public.schema_version(singleton, version) VALUES (TRUE, 1);
