\set ON_ERROR_STOP on
-- Идентификаторы существуют только в изолированной тестовой базе PostgreSQL.
set role authenticated;
select set_config('request.uid','00000000-0000-4000-8000-000000000001',false);
select set_config('request.jwt','{"app_metadata":{"provider":"github"}}',false);
insert into public.программы(автор, название, исходник) values (auth.uid(), 'пример', 'сказать(42);');
do $$ begin
  begin
    insert into public.программы(автор, название, исходник) values ('00000000-0000-4000-8000-000000000002', 'подмена', 'тест');
    raise exception 'подмена автора разрешена';
  exception when insufficient_privilege then null; end;
  begin
    update public.программы set скрыто = true;
    raise exception 'изменение скрыто разрешено';
  exception when insufficient_privilege then null; end;
  begin
    insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
    raise exception 'жалоба на свою программу разрешена';
  exception when insufficient_privilege then null; end;
end $$;
select set_config('request.uid','00000000-0000-4000-8000-000000000002',false);
select set_config('request.jwt','{"app_metadata":{"provider":"email"}}',false);
do $$ begin
  begin
    insert into public.темы(автор, название, текст) values (auth.uid(), 'тема', 'текст');
    raise exception 'публикация не через GitHub разрешена';
  exception when insufficient_privilege then null; end;
end $$;
select set_config('request.jwt','{"app_metadata":{"provider":"github"}}',false);
insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
do $$ begin
  begin
    insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
    raise exception 'повторная жалоба разрешена';
  exception when unique_violation then null; end;
end $$;
select set_config('request.uid','00000000-0000-4000-8000-000000000003',false);
insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
select set_config('request.uid','00000000-0000-4000-8000-000000000004',false);
insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
select set_config('request.uid','00000000-0000-4000-8000-000000000005',false);
insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
do $$ begin if (select count(*) from public.программы) <> 1 then raise exception 'скрыто до пяти жалоб'; end if; end $$;
select set_config('request.uid','00000000-0000-4000-8000-000000000006',false);
insert into public.жалобы_программы(автор, цель) select auth.uid(), id from public.программы;
do $$ begin if (select count(*) from public.программы) <> 0 then raise exception 'пять жалоб не скрыли программу'; end if; end $$;
reset role;
do $$ begin if not (select скрыто from public.программы limit 1) then raise exception 'скрытие отсутствует в базе'; end if; end $$;
